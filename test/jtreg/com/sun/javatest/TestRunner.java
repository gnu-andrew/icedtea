/*
 * $Id$
 *
 * Copyright 1996-2008 Sun Microsystems, Inc.  All Rights Reserved.
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
package com.sun.javatest;

import java.util.Iterator;

import com.sun.javatest.util.BackupPolicy;

/**
 * TestRunner is the abstract base class providing the ability to
 * control how tests are run. A default is provided that executes
 * each test by means of a script class.
 * <p>If a test suite does not wish tom use the default test runner,
 * it should provide its own implementation of this class. The primary
 * method which should be implemented is {@link #runTests}.
 */
public abstract class TestRunner
{
    /**
     * Set the work directory used to store the test results generated
     * by this test runner.
     * @param wd the work directory used to store the test results generated
     * by this test runner
     * @see #getWorkDirectory
     */
    void setWorkDirectory(WorkDirectory wd) {
        workDir = wd;
    }

    /**
     * Get the work directory to be used to store the test results generated
     * by this test runner.
     * @return the work directory used to store the test results generated
     * by this test runner
     */
    public WorkDirectory getWorkDirectory() {
        return workDir;
    }

    /**
     * Get the test suite containing the tests to be run by this test runner.
     * @return  the test suite containing the tests to be run by this test runner
     */
    public TestSuite getTestSuite() {
        return workDir.getTestSuite();
    }

    /**
     * Set the backup policy to be used when creating the test result files
     * generated by this test runner.
     * @param backupPolicy the backup policy to be used when creating the
     * test result files generated by this test runner
     * @see #getBackupPolicy
     * @see TestResult#writeResults(WorkDirectory, BackupPolicy)
     */
    void setBackupPolicy(BackupPolicy backupPolicy) {
        this.backupPolicy = backupPolicy;
    }

    /**
     * Get the backup policy to be used when creating the test result files
     * generated by this test runner.
     * @return the backup policy to be used when creating the
     * test result files generated by this test runner
     * @see TestResult#writeResults(WorkDirectory, BackupPolicy)
     */
    public BackupPolicy getBackupPolicy() {
        return backupPolicy;
    }

    /**
     * Set the test environment to be used to execute the tests that will
     * be run by this test runner.
     * @param env the test environment to be used to execute the tests that will
     * be run by this test runner
     * @see #getEnvironment
     */
    void setEnvironment(TestEnvironment env) {
        this.env = env;
    }

    /**
     * Get the test environment to be used to execute the tests that will
     * be run by this test runner.
     * @return the test environment to be used to execute the tests that will
     * be run by this test runner
     * @see #getEnvironment
     */
    public TestEnvironment getEnvironment() {
        return env;
    }

    /**
     * Set the exclude list to be used to identify the test cases to be
     * excluded when running tests.
     * @param excludeList the exclude list to be used to identify the test cases to be
     * excluded when running tests
     * @see #getExcludeList
     * @see #getExcludedTestCases
     */
    void setExcludeList(ExcludeList excludeList) {
        this.excludeList = excludeList;
    }

    /**
     * Get the exclude list to be used to identify the test cases to be
     * excluded when running tests.
     * @return the exclude list to be used to identify the test cases to be
     * excluded when running tests
     * @see #getExcludedTestCases
     */
    public ExcludeList getExcludeList() {
        return excludeList;
    }

    /**
     * Get the names of the test cases to be excluded when running a specific test.
     * @param td the test for which the excluded test cases should be obtained
     * @return the names of the test cases to be excluded when running a specific test.
     * The result is null if the test is not found or is
     * completely excluded without specifying test cases.  This may be
     * a mix of single TC strings or a comma separated list of them.
     */
    public String[] getExcludedTestCases(TestDescription td) {
        return excludeList.getTestCases(td);
    }

    /**
     * Set the concurrency to be used when running the tests, if applicable.
     * @param conc the concurrency to be used when running the tests, if applicable
     * @see #getConcurrency
     */
    void setConcurrency(int conc) {
        concurrency = conc;
    }

    /**
     * Get the concurrency to be used when running the tests, if applicable.
     * @return the concurrency to be used when running the tests, if applicable
     * @see #getConcurrency
     */
    public int getConcurrency() {
        return concurrency;
    }


    /**
     * Set the notifier to be used when running the tests.
     * @param notifier the notifier to be used when running the tests
     * @see #notifyStartingTest
     * @see #notifyFinishedTest
     */
    void setNotifier(Harness.Observer notifier) {
        this.notifier = notifier;
    }

    /**
     * Run the tests obtained from an iterator. The iterator returns
     * TestDescription objects for the tests that have been selected to be run.
     * The iterator supports the standard hasNext() and next() methods;
     * it does not support remove(), which throws UnsupportedOperationException.
     * Each test description gives the details of the test to be run.
     * As each test is started, the implementation of this method must create
     * a new TestResult object and call {@link #notifyStartingTest}.
     * When the test completes (however it completes) the implementation of
     * this method must call {@link #notifyFinishedTest}.
     * @param testIter the iterator to be used to obtain the tests to be run
     * @return true if and only if all the tests executed successfully and passed
     * @throws InterruptedException if the test run was interrupted
     */
    protected abstract boolean runTests(Iterator testIter) throws InterruptedException;

    /**
     * This method must be called as each test is started, to notify any
     * registered observers.
     * @param tr the TestResult object for the test that has been started
     * @see Harness.Observer
     */
    protected void notifyStartingTest(TestResult tr) {
        notifier.startingTest(tr);
    }

    /**
     * This method must be called when each test is finished, to notify any
     * registered observers.
     * @param tr the TestResult object for the test that has been started
     * @see Harness.Observer
     */
    protected void notifyFinishedTest(TestResult tr) {
        // this must do the following:
        // numTestsDone++;
        // resultTable.update(result);
        // notify observers
        notifier.finishedTest(tr);
    }

    private WorkDirectory workDir;
    private BackupPolicy backupPolicy;
    private TestEnvironment env;
    private ExcludeList excludeList;
    private int concurrency;
    private Harness.Observer notifier;
}
