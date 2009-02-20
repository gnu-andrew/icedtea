/*
 * Copyright 2007-2008 Sun Microsystems, Inc.  All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Sun designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Sun in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 */

package sun.nio.ch;

import java.nio.channels.Channel;
import java.io.IOException;
import java.io.FileDescriptor;
import java.util.Queue;

import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.TimeUnit;

import java.util.concurrent.locks.*;
import java.util.concurrent.atomic.AtomicInteger;
import java.security.PrivilegedAction;
import java.security.AccessController;
import java.security.AccessControlContext;
import sun.security.action.GetPropertyAction;

import java.nio.channels.AsynchronousChannelGroup;
import java.nio.channels.spi.AsynchronousChannelProvider;

import java.util.concurrent.ScheduledThreadPoolExecutor;

/**
 * Base implementation of AsynchronousChannelGroup
 */

abstract class AsynchronousChannelGroupImpl
    extends AsynchronousChannelGroup implements Executor
{
    // number of internal threads handling I/O events when using an unbounded
    // thread pool. Internal threads do not dispatch to completion handlers.
    private static final int INTERNAL_THREAD_COUNT;
    static {
        String propValue = AccessController.doPrivileged(
            new GetPropertyAction("sun.nio.ch.internalThreadPoolSize"));
        int n = -1;
        try {
            n = Integer.parseInt(propValue);
        } catch (NumberFormatException e) { }
        INTERNAL_THREAD_COUNT = (n >= 0) ? n : 1;
    }

    // associated thread pool
    private final ThreadPool pool;

    // number of tasks running (including internal)
    private final AtomicInteger threadCount = new AtomicInteger();

    // associated Executor for timeouts
    private ScheduledThreadPoolExecutor timeoutExecutor;

    // task queue for when using a fixed thread pool. In that case, thread
    // waiting on I/O events must be awokon to poll tasks from this queue.
    private final Queue<Runnable> taskQueue;

    // group shutdown
    // shutdownLock is RW lock so as to allow for concurrent queuing of tasks
    // when using a fixed thread pool.
    private final ReadWriteLock shutdownLock = new ReentrantReadWriteLock();
    private final Object shutdownNowLock = new Object();
    private volatile boolean shutdown;
    private volatile boolean terminateInitiated;

    AsynchronousChannelGroupImpl(AsynchronousChannelProvider provider,
                                 ThreadPool pool)
    {
        super(provider);
        this.pool = pool;

        if (pool.isFixedThreadPool()) {
            taskQueue = new ConcurrentLinkedQueue<Runnable>();
        } else {
            taskQueue = null;   // not used
        }

        // use default thread factory as thread should not be visible to
        // application (it doesn't execute completion handlers).
        this.timeoutExecutor =
	  new ScheduledThreadPoolExecutor(1, ThreadPool.defaultThreadFactory());
        this.timeoutExecutor.setRemoveOnCancelPolicy(true);
    }

    final ExecutorService executor() {
        return pool.executor();
    }

    final boolean isFixedThreadPool() {
        return pool.isFixedThreadPool();
    }

    final int fixedThreadCount() {
        if (isFixedThreadPool()) {
            return pool.poolSize();
        } else {
            return pool.poolSize() + INTERNAL_THREAD_COUNT;
        }
    }

    private Runnable bindToGroup(final Runnable task) {
        final AsynchronousChannelGroupImpl thisGroup = this;
        return new Runnable() {
            public void run() {
                Invoker.bindToGroup(thisGroup);
                task.run();
            }
        };
    }

    private void startInternalThread(final Runnable task) {
        AccessController.doPrivileged(new PrivilegedAction<Void>() {

            public Void run() {
                // internal threads should not be visible to application so
                // cannot use user-supplied thread factory
                ThreadPool.defaultThreadFactory().newThread(task).start();
                return null;
            }
         });
    }

    protected final void startThreads(Runnable task) {
        if (!isFixedThreadPool()) {
            for (int i=0; i<INTERNAL_THREAD_COUNT; i++) {
                startInternalThread(task);
                threadCount.incrementAndGet();
            }
        }
        if (pool.poolSize() > 0) {
            task = bindToGroup(task);
            try {
                for (int i=0; i<pool.poolSize(); i++) {
                    pool.executor().execute(task);
                    threadCount.incrementAndGet();
                }
            } catch (RejectedExecutionException  x) {
                // nothing we can do
            }
        }
    }

    final int threadCount() {
        return threadCount.get();
    }

    /**
     * Invoked by tasks as they terminate
     */
    final int threadExit(Runnable task, boolean replaceMe) {
        if (replaceMe) {
            try {
                if (Invoker.isBoundToAnyGroup()) {
                    // submit new task to replace this thread
                    pool.executor().execute(bindToGroup(task));
                } else {
                    // replace internal thread
                    startInternalThread(task);
                }
                return threadCount.get();
            } catch (RejectedExecutionException x) {
                // unable to replace
            }
        }
        return threadCount.decrementAndGet();
    }

    /**
     * Wakes up a thread waiting for I/O events to execute the given task.
     */
    abstract void executeOnHandlerTask(Runnable task);

    /**
     * For a fixed thread pool the task is queued to a thread waiting on I/O
     * events. For other thread pools we simply submit the task to the thread
     * pool.
     */
    final void executeOnPooledThread(Runnable task) {
        if (isFixedThreadPool()) {
            executeOnHandlerTask(task);
        } else {
            pool.executor().execute(bindToGroup(task));
        }
    }

    final void offerTask(Runnable task) {
        taskQueue.offer(task);
    }

    final Runnable pollTask() {
        return (taskQueue == null) ? null : taskQueue.poll();
    }

    final Future<?> schedule(Runnable task, long timeout, TimeUnit unit) {
        try {
            return timeoutExecutor.schedule(task, timeout, unit);
        } catch (RejectedExecutionException rej) {
            if (terminateInitiated) {
                // no timeout scheduled as group is terminating
                return null;
            }
            throw new AssertionError(rej);
        }
    }


    public final boolean isShutdown() {
        return shutdown;
    }


    public final boolean isTerminated()  {
        return pool.executor().isTerminated();
    }

    /**
     * Returns true if there are no channels in the group
     */
    abstract boolean isEmpty();

    /**
     * Attaches a foreign channel to this group.
     */
    abstract Object attachForeignChannel(Channel channel, FileDescriptor fdo)
        throws IOException;

    /**
     * Detaches a foreign channel from this group.
     */
    abstract void detachForeignChannel(Object key);

    /**
     * Closes all channels in the group
     */
    abstract void closeAllChannels() throws IOException;

    /**
     * Shutdown all tasks waiting for I/O events.
     */
    abstract void shutdownHandlerTasks();

    private void shutdownExecutors() {
        AccessController.doPrivileged(new PrivilegedAction<Void>() {
            public Void run() {
                pool.executor().shutdown();
                timeoutExecutor.shutdown();
                return null;
            }
        });
    }


    public final void shutdown() {
        shutdownLock.writeLock().lock();
        try {
            if (shutdown) {
                // already shutdown
                return;
            }
            shutdown = true;
        } finally {
            shutdownLock.writeLock().unlock();
        }

        // if there are channels in the group then shutdown will continue
        // when the last channel is closed
        if (!isEmpty()) {
            return;
        }
        // initiate termination (acquire shutdownNowLock to ensure that other
        // threads invoking shutdownNow will block).
        synchronized (shutdownNowLock) {
            if (!terminateInitiated) {
                terminateInitiated = true;
                shutdownHandlerTasks();
                shutdownExecutors();
            }
        }
    }


    public final void shutdownNow() throws IOException {
        shutdownLock.writeLock().lock();
        try {
            shutdown = true;
        } finally {
            shutdownLock.writeLock().unlock();
        }
        synchronized (shutdownNowLock) {
            if (!terminateInitiated) {
                terminateInitiated = true;
                closeAllChannels();
                shutdownHandlerTasks();
                shutdownExecutors();
            }
        }
    }


    public final boolean awaitTermination(long timeout, TimeUnit unit)
        throws InterruptedException
    {
        return pool.executor().awaitTermination(timeout, unit);
    }

    /**
     * Executes the given command on one of the channel group's pooled threads.
     */

    public final void execute(Runnable task) {
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            // when a security manager is installed then the user's task
            // must be run with the current calling context
            final AccessControlContext acc = AccessController.getContext();
            final Runnable delegate = task;
            task = new Runnable() {

                public void run() {
                    AccessController.doPrivileged(new PrivilegedAction<Void>() {

                        public Void run() {
                            delegate.run();
                            return null;
                        }
                    }, acc);
                }
            };
        }
        executeOnPooledThread(task);
    }
}
