/* PulseAudioClip.java
   Copyright (C) 2008 Red Hat, Inc.

This file is part of IcedTea.

IcedTea is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 2.

IcedTea is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with IcedTea; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301 USA.

Linking this library statically or dynamically with other modules is
making a combined work based on this library.  Thus, the terms and
conditions of the GNU General Public License cover the whole
combination.

As a special exception, the copyright holders of this library give you
permission to link this library with independent modules to produce an
executable, regardless of the license terms of these independent
modules, and to copy and distribute the resulting executable under
terms of your choice, provided that you also meet, for each linked
independent module, the terms and conditions of the license of that
module.  An independent module is a module which is not derived from
or based on this library.  If you modify this library, you may extend
this exception to your version of the library, but you are not
obligated to do so.  If you do not wish to do so, delete this
exception statement from your version.
 */

package org.classpath.icedtea.pulseaudio;

import java.util.concurrent.Semaphore;

import javax.sound.sampled.AudioFormat;
import javax.sound.sampled.AudioSystem;
import javax.sound.sampled.DataLine;
import javax.sound.sampled.LineEvent;
import javax.sound.sampled.LineUnavailableException;

import org.classpath.icedtea.pulseaudio.Stream.WriteListener;

public abstract class PulseAudioDataLine extends PulseAudioLine implements
		DataLine {

	protected static final int DEFAULT_BUFFER_SIZE = StreamBufferAttributes.SANE_DEFAULT;
	protected static final String PULSEAUDIO_FORMAT_KEY = "PulseAudioFormatKey";

	protected String streamName = "Java Stream";

	// true between start() and stop()
	protected boolean isStarted = false;

	// true between a started and an underflow callback
	protected boolean dataWritten = false;

	// true if a stream has been paused
	// protected boolean isPaused = false;

	protected AudioFormat[] supportedFormats = null;
	protected AudioFormat currentFormat = null;
	protected AudioFormat defaultFormat = null;
	protected boolean sendEvents = true;

	protected int bufferSize = 0;
	// the total number of frames played since this line was opened
	protected long framesSinceOpen = 0;

	protected EventLoop eventLoop = null;
	protected Semaphore semaphore = new Semaphore(0);
	protected Stream stream;
	boolean writeInterrupted = false;

	protected void open(AudioFormat format, int bufferSize)
			throws LineUnavailableException {

		if (isOpen) {
			throw new IllegalStateException("Line is already open");
		}
		
		PulseAudioMixer mixer = PulseAudioMixer.getInstance();
		if(!mixer.isOpen()) {
			mixer.open();
		}

		eventLoop = EventLoop.getEventLoop();

		createStream(format);
		addStreamListeners();
		connect(null, bufferSize);
	}

	private void createStream(AudioFormat format)
			throws LineUnavailableException {

		for (AudioFormat myFormat : supportedFormats) {
			if (format.matches(myFormat)) {
				/*
				 * A few issues with format:
				 * 
				 * To match: SAME encoding: safe because its a java enum. SAME
				 * number of channels: safe because myFormat has specific
				 * values. SAME bits per sample (aka sampleSize) and bytes per
				 * frame (aka frameSize): safe because myFormat has specific
				 * values. SAME sample rate: _not_ safe because myFormat uses
				 * AudioSystem.NOT_SPECIFIED. SAME frame rate: safe because we
				 * _ignore_ it completely ;)
				 * 
				 * 
				 */

				float sampleRate = format.getSampleRate();
				if (sampleRate == (float) AudioSystem.NOT_SPECIFIED) {
					/* pick a random sample rate */
					sampleRate = 44100.0f;
				}

				synchronized (eventLoop.threadLock) {
					stream = new Stream(eventLoop.getContextPointer(),
							streamName, Stream.Format.valueOf((String) myFormat
									.getProperty(PULSEAUDIO_FORMAT_KEY)),
							(int) sampleRate, myFormat.getChannels());

				}
				currentFormat = format;
				isOpen = true;
			}
		}

		if (!isOpen) {
			throw new IllegalArgumentException("Invalid format");
		}

		// System.out.println("Stream " + stream + " created");

	}

	private void addStreamListeners() {
		Stream.StateListener openCloseListener = new Stream.StateListener() {

			@Override
			public void update() {
				synchronized (eventLoop.threadLock) {

					/*
					 * Note the order: first we notify all the listeners, and
					 * then return. this means no race conditions when the
					 * listeners are removed before they get called by close
					 * 
					 * eg:
					 * 
					 * line.close(); line.removeLineListener(listener)
					 * 
					 * the listener is guaranteed to have run
					 * 
					 */

					if (stream.getState() == Stream.State.READY) {
						if (sendEvents) {
							fireLineEvent(new LineEvent(
									PulseAudioDataLine.this,
									LineEvent.Type.OPEN, framesSinceOpen));
						}
						semaphore.release();

					} else if (stream.getState() == Stream.State.TERMINATED
							|| stream.getState() == Stream.State.FAILED) {
						if (sendEvents) {
							fireLineEvent((new LineEvent(
									PulseAudioDataLine.this,
									LineEvent.Type.CLOSE, framesSinceOpen)));
						}
						synchronized (this) {
							this.notifyAll();
						}
						semaphore.release();

					}
				}
			}
		};

		stream.addStateListener(openCloseListener);

		Stream.UnderflowListener stoppedListener = new Stream.UnderflowListener() {
			@Override
			public void update() {
				dataWritten = false;

				// always send a STOP event on an underflow (assumption:
				// an underflow can't happen while the stream is corked)
				fireLineEvent(new LineEvent(PulseAudioDataLine.this,
						LineEvent.Type.STOP, framesSinceOpen));
			}
		};
		stream.addUnderflowListener(stoppedListener);

		Stream.PlaybackStartedListener startedListener = new Stream.PlaybackStartedListener() {
			@Override
			public void update() {
				fireLineEvent(new LineEvent(PulseAudioDataLine.this,
						LineEvent.Type.START, framesSinceOpen));

				dataWritten = true;
				synchronized (this) {
					this.notifyAll();
				}

			}
		};

		stream.addPlaybackStartedListener(startedListener);

		WriteListener writeNotifier = new WriteListener() {

			@Override
			public void update() {
				synchronized (eventLoop.threadLock) {
					eventLoop.threadLock.notifyAll();
				}
			}

		};
		stream.addWriteListener(writeNotifier);

		Stream.CorkListener corkListener = new Stream.CorkListener() {

			@Override
			public void update() {
				synchronized (eventLoop.threadLock) {
					eventLoop.threadLock.notifyAll();
				}
			}

		};
		stream.addCorkListener(corkListener);
	}

	private void connect(Stream masterStream, int bufferSize)
			throws LineUnavailableException {

		try {
			synchronized (eventLoop.threadLock) {
				connectLine(bufferSize, masterStream);
			}
		} catch (LineUnavailableException e) {
			// error connecting to the server!
			// stream.removePlaybackStartedListener(startedListener);
			// stream.removeUnderflowListener(stoppedListener);
			// stream.removeStateListener(openCloseListener);
			stream.free();
			stream = null;
			throw e;

		}
		this.bufferSize = bufferSize;
		try {
			semaphore.acquire();
			synchronized (eventLoop.threadLock) {
				if (stream.getState() != Stream.State.READY) {
					stream.disconnect();
					stream.free();
					throw new LineUnavailableException(
							"unable to obtain a line");
				}
			}
		} catch (InterruptedException e) {
			throw new LineUnavailableException("unable to prepare stream");
		}
	}

	protected void open(AudioFormat format) throws LineUnavailableException {
		open(format, DEFAULT_BUFFER_SIZE);

	}

	public void open() throws LineUnavailableException {
		assert (defaultFormat != null);
		open(defaultFormat, DEFAULT_BUFFER_SIZE);
	}

	public void close() {

		if (!isOpen) {
			throw new IllegalStateException(
					"Line must be open for close() to work");
		}

		synchronized (eventLoop.threadLock) {
			stream.disconnect();
		}

		try {
			semaphore.acquire();
		} catch (InterruptedException e) {
			throw new RuntimeException("unable to prepare stream");
		}

		synchronized (eventLoop.threadLock) {
			stream.free();
		}

		super.close();

		isStarted = false;
	}

	public void reconnectforSynchronization(Stream masterStream)
			throws LineUnavailableException {
		sendEvents = false;
		drain();

		synchronized (eventLoop.threadLock) {
			stream.disconnect();
		}
		try {
			semaphore.acquire();
		} catch (InterruptedException e) {
			throw new RuntimeException("unable to prepare stream");
		}

		createStream(getFormat());
		addStreamListeners();
		connect(masterStream, getBufferSize());

		sendEvents = true;
	}

	public void start() {
		if (!isOpen) {
			throw new IllegalStateException(
					"Line must be open()ed before it can be start()ed");
		}

		if (isStarted) {
			return;

		}

		Operation op;
		synchronized (eventLoop.threadLock) {
			op = stream.unCork();
		}

		op.waitForCompletion();
		op.releaseReference();
		isStarted = true;

	}

	public synchronized void stop() {
		if (!isOpen) {
			throw new IllegalStateException(
					"Line must be open()ed before it can be start()ed");

		}
		writeInterrupted = true;
		if (!isStarted) {
			return;
		}

		Operation op;
		synchronized (eventLoop.threadLock) {
			op = stream.cork();
			// if there are no data on the line when stop was called,
			// don't send a stop event
			if (dataWritten && (isStarted)) {
				fireLineEvent(new LineEvent(PulseAudioDataLine.this,
						LineEvent.Type.STOP, framesSinceOpen));
			}
		}

		op.waitForCompletion();
		op.releaseReference();

		isStarted = false;
	}

	/*
	 * TODO
	 * 
	 * http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=4791152 :
	 * 
	 * a line is active in between calls to start() and stop(). In that sense,
	 * active means that the line is ready to take or give data. Running is
	 * tightly bound to data flow in the line. I.e. when you start a
	 * SourceDataLine but never write data to it, the line should not be
	 * running. This also means that a line should become not running on buffer
	 * underrun/overflow.
	 * 
	 * 
	 * HOWEVER, the javadocs say the opposite thing! (need help from the jck =
	 * official spec)
	 * 
	 * 
	 */

	public boolean isActive() {
		return isStarted;
	}

	public boolean isRunning() {
		return isStarted && dataWritten;
	}

	protected abstract void connectLine(int bufferSize, Stream masterStream)
			throws LineUnavailableException;

	public abstract void drain();

	public Stream getStream() {
		if (!isOpen) {
			throw new IllegalStateException("Line must be open");
		}

		return stream;
	}

	@Override
	public int getBufferSize() {
		if (!isOpen) {
			return DEFAULT_BUFFER_SIZE;
		}
		return bufferSize;
	}

	@Override
	public AudioFormat getFormat() {
		if (!isOpen) {
			return defaultFormat;
		}
		return currentFormat;
	}

	public float getLevel() {
		return AudioSystem.NOT_SPECIFIED;
	}

	public void setName(String streamName) {
		if (isOpen) {

			Operation o;
			synchronized (eventLoop.threadLock) {
				o = stream.setName(streamName);
			}
			o.waitForCompletion();
			o.releaseReference();

		}

		this.streamName = streamName;

	}

	public String getName() {
		return streamName;
	}

	public int getBytesInBuffer() {
		Operation o;
		synchronized (eventLoop.threadLock) {
			o = stream.updateTimingInfo();
		}
		o.waitForCompletion();
		o.releaseReference();
		return stream.bytesInBuffer();
	}

}
