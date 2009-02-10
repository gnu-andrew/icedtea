/*
 * Copyright 2007-2008 Sun Microsystems, Inc.  All Rights Reserved.
 * Copyright 2009 Red Hat, Inc.
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

package org.classpath.icedtea.java.nio.file;

import java.util.Set;
import java.io.Closeable;
import java.io.IOException;

import org.classpath.icedtea.java.nio.file.attribute.UserPrincipalLookupService;

import org.classpath.icedtea.java.nio.file.spi.FileSystemProvider;

/**
 * Provides an interface to a file system and is the factory for objects to
 * access files and other objects in the file system.
 *
 * <p> The default file system, obtained by invoking the {@link FileSystems#getDefault
 * FileSystems.getDefault} method, provides access to the file system that is
 * accessible to the Java virtual machine. The {@link FileSystems} class defines
 * methods to create file systems that provide access to other types of file
 * systems.
 *
 * <p> A file system is the factory for several types of objects:
 *
 * <ul>
 *   <li><p> The {@link #getPath getPath} method converts a system dependent
 *     <em>path string</em>, returning a {@link Path} object that may be used
 *     to locate and access a file. </p></li>
 *   <li><p> The {@link #getNameMatcher  getNameMatcher} method is used
 *     to create a {@link PathMatcher} that performs match operations on
 *     file names. </p></li>
 *   <li><p> The {@link #getFileStores getFileStores} method returns an iterator
 *     over the underlying {@link FileStore file-stores}. </p></li>
 *   <li><p> The {@link #getUserPrincipalLookupService getUserPrincipalLookupService}
 *     method returns the {@link UserPrincipalLookupService} to lookup users or
 *     groups by name. </p></li>
 *   <li><p> The {@link #newWatchService newWatchService} method creates a
 *     {@link WatchService} that may be used to watch objects for changes and
 *     events. </p></li>
 * </ul>
 *
 * <p> File systems vary greatly. In some cases the file system is a single
 * hierarchy of files with one top-level root directory. In other cases it may
 * have several distinct file hierarchies, each with its own top-level root
 * directory. The {@link #getRootDirectories getRootDirectories} method may be
 * used to iterate over the root directories in the file system. A file system
 * is typically composed of one or more underlying {@link FileStore file-stores}
 * that provide the storage for the files. Theses file stores can also vary in
 * the features they support, and the file attributes or <em>meta-data</em> that
 * they associate with files.
 *
 * <p> A file system is open upon creation and can be closed by invoking its
 * {@link #close() close} method. Once closed, any further attempt to access
 * objects in the file system cause {@link ClosedFileSystemException} to be
 * thrown. File systems created by the default {@link FileSystemProvider provider}
 * cannot be closed.
 *
 * <p> A {@code FileSystem} can provide read-only or read-write access to the
 * file system. Whether or not a file system provides read-only access is
 * established when the {@code FileSystem} is created and can be tested by invoking
 * its {@link #isReadOnly() isReadOnly} method. Attempts to write to file stores
 * by means of an object associated with a read-only file system throws {@link
 * ReadOnlyFileSystemException}.
 *
 * <p> File systems are safe for use by multiple concurrent threads. The {@link
 * #close close} method may be invoked at any time to close a file system but
 * whether a file system is <i>asynchronously closeable</i> is provider specific
 * and therefore unspecified. In other words, if a thread is accessing an
 * object in a file system, and another thread invokes the {@code close} method
 * then it may require to block until the first operation is complete. Closing
 * a file system causes all open channels, watch services, and other {@link
 * Closeable closeable} objects associated with the file system to be closed.
 *
 * @since 1.7
 */

public abstract class FileSystem
    implements Closeable
{
    /**
     * Initializes a new instance of this class.
     */
    protected FileSystem() {
    }

    /**
     * Returns the provider that created this file system.
     *
     * @return  The provider that created this file system.
     */
    public abstract FileSystemProvider provider();

    /**
     * Closes this file system.
     *
     * <p> After a file system is closed then all subsequent access to the file
     * system, either by methods defined by this class or on objects associated
     * with this file system, throw {@link ClosedFileSystemException}. If the
     * file system is already closed then invoking this method has no effect.
     *
     * <p> Closing a file system will close all open {@link
     * java.nio.channels.Channel channels}, {@link DirectoryStream directory-streams},
     * {@link WatchService watch-service}, and other closeable objects associated
     * with this file system. The {@link FileSystems#getDefault default} file
     * system cannot be closed.
     *
     * @throws  IOException
     *          If an I/O error occurs
     * @throws  UnsupportedOperationException
     *          Thrown in the case of the default file system
     */

    public abstract void close() throws IOException;

    /**
     * Tells whether or not this file system is open.
     *
     * <p> File systems created by the default provider are always open.
     *
     * @return  {@code true} if, and only if, this file system is open
     */
    public abstract boolean isOpen();

    /**
     * Tells whether or not this file system allows only read-only access to
     * its file stores.
     *
     * @return  {@code true} if, and only if, this file system provides
     *          read-only access
     */
    public abstract boolean isReadOnly();

    /**
     * Returns the name separator, represented as a string.
     *
     * <p> The name separator is used to separate names in a path string. An
     * implementation may support multiple name separators in which case this
     * method returns an implementation specific <em>default</em> name separator.
     * This separator is used when creating path strings by invoking the {@link
     * Path#toString() toString()} method.
     *
     * <p> In the case of the default provider, this method returns the same
     * separator as {@link java.io.File#separator}.
     *
     * @return  The name separator
     */
    public abstract String getSeparator();

    /**
     * Returns an object to iterate over the paths of the root directories.
     *
     * <p> A file system provides access to a file store that may be composed
     * of a number of distinct file hierarchies, each with its own top-level
     * root directory. Unless denied by the security manager, each element in
     * the returned iterator corresponds to the root directory of a distinct
     * file hierarchy. The order of the elements is not defined. The file
     * hierarchies may change during the lifetime of the Java virtual machine.
     * For example, in some implementations, the insertion of removable media
     * may result in the creation of a new file hierarchy with its own
     * top-level directory.
     *
     * <p> When a security manager is installed, it is invoked to check access
     * to the each root directory. If denied, the root directory is not returned
     * by the iterator. In the case of the default provider, the {@link
     * SecurityManager#checkRead(String)} method is invoked to check read access
     * to each root directory. It is system dependent if the permission checks
     * are done when the iterator is obtained or during iteration.
     *
     * @return  An object to iterate over the root directories
     */
    public abstract Iterable<Path> getRootDirectories();

    /**
     * Returns an object to iterate over the underlying file stores.
     *
     * <p> The elements of the returned iterator are the {@link
     * FileStore FileStores} for this file system. The order of the elements is
     * not defined and the file stores may change during the lifetime of the
     * Java virtual machine. When an I/O error occurs, perhaps because a file
     * store is not accessible, then it is not returned by the iterator.
     *
     * <p> When a security manager is installed, it is invoked to check access
     * to each file store. If denied, the file store is not returned by the
     * iterator. In the case of the default provider, the {@link
     * SecurityManager#checkRead(String)} method is invoked to check read access
     * to the file store's <em>top-most</em> directory. It is system dependent
     * if the permission checks are done when the iterator is obtained or during
     * iteration.
     *
     * <p> <b>Usage Example:</b>
     * Suppose we want to print the space usage for all file stores:
     * <pre>
     *     for (FileStore store: FileSystems.getDefault().getFileStores()) {
     *         FileStoreSpaceAttributes attrs = Attributes.readFileStoreSpaceAttributes(store);
     *         long total = attrs.totalSpace() / 1024;
     *         long used = (attrs.totalSpace() - attrs.unallocatedSpace()) / 1024;
     *         long avail = attrs.usableSpace() / 1024;
     *         System.out.format("%-20s %12d %12d %12d%n", store, total, used, avail);
     *     }
     * </pre>
     *
     * @return  An object to iterate over the backing file stores
     */
    public abstract Iterable<FileStore> getFileStores();

    /**
     * Returns the set of the {@link FileAttributeView#name names} of the file
     * attribute views supported by this {@code FileSystem}.
     *
     * <p> The {@link BasicFileAttributeView} is required to be supported and
     * therefore the set contains at least one element, "basic".
     *
     * <p> The {@link FileStore#supportsFileAttributeView(String)
     * supportsFileAttributeView(String)} method may be used to test if an
     * underlying {@link FileStore} supports the file attributes identified by a
     * file attribute view.
     *
     * @return  An unmodifiable set of the names of the supported file attribute
     *          views
     */
    public abstract Set<String> supportedFileAttributeViews();

    /**
     * Converts a path string to a {@code Path}.
     *
     * <p> The parsing and conversion to a path object is inherently
     * implementation dependent. In the simplest case, the path string is rejected,
     * and {@link InvalidPathException} thrown, if the path string contains
     * characters that cannot be converted to characters that are <em>legal</em>
     * to the file store. For example, on UNIX systems, the NUL (&#92;u0000)
     * character is not allowed to be present in a path. An implementation may
     * choose to reject path strings that contain names that are longer than those
     * allowed by any file store, and where an implementation supports a complex
     * path syntax, it may choose to reject path strings that are <em>badly
     * formed</em>.
     *
     * <p> In the case of the default provider, path strings are parsed based
     * on the definition of paths at the platform or virtual file system level.
     * For example, an operating system may not allow specific characters to be
     * present in a file name, but a specific underlying file store may impose
     * different or additional restrictions on the set of legal
     * characters.
     *
     * <p> This method throws {@link InvalidPathException} when the path string
     * cannot be converted to a path. Where possible, and where applicable,
     * the exception is created with an {@link InvalidPathException#getIndex
     * index} value indicating the first position in the {@code path} parameter
     * that caused the path string to be rejected.
     *
     * <p> Invoking this method with an empty path string throws
     * {@code InvalidPathException}.
     *
     * @param   path
     *          The path string
     *
     * @return  A {@code Path} object
     *
     * @throws  InvalidPathException
     *          If the path string cannot be converted
     */
    public abstract Path getPath(String path);

    /**
     * Returns a {@code PathMatcher} that performs match operations on file
     * {@link Path#getName names} by interpreting a given pattern.
     *
     * <p> The {@code syntax} parameter identifies the syntax. This method
     * supports the "{@code glob}" and "{@code regex}" syntaxes, and may
     * support others. The value of the syntax component is compared without
     * regard to case.
     *
     * <p> When the syntax is "{@code glob}" then the {@code String}
     * representation of the file name is matched using a limited pattern
     * language that resembles regular expressions but with a simpler syntax.
     * For example:
     *
     * <blockquote>
     * <table border="0">
     * <tr>
     *   <td>{@code *.java}</td>
     *   <td>Matches file names ending in {@code .java}</td>
     * </tr>
     * <tr>
     *   <td>{@code *.*}</td>
     *   <td>Matches file names containing a dot</td>
     * </tr>
     * <tr>
     * <tr>
     *   <td>{@code *.{java,class}}</td>
     *   <td>Matches file names ending with {@code .java} or {@code .class}</td>
     * </tr>
     * <tr>
     *   <td>{@code foo.?}</td>
     *   <td>Matches file names starting with {@code foo.} and a single
     *   character extension</td>
     * </tr>
     * </table>
     * </blockquote>
     *
     * <p> The following rules are used to interprete glob patterns:
     *
     * <p> <ul>
     *   <li><p> The {@code *} character matches zero or more {@link Character
     *   characters}. </p></li>
     *
     *   <li><p> The {@code ?} character matches exactly one character. </p></li>
     *
     *   <li><p> The backslash character ({@code \}) is used to escape characters
     *   that would otherwise be interpreted as special characters. The expression
     *   {@code \\} matches a single backslash and "\{" matches a left brace
     *   for example.  </p></li>
     *
     *   <li><p> The {@code [ ]} characters are a <i>bracket expression</i> and
     *   match a single character that is contained within the brackets. For
     *   example, {@code [abc]} matches {@code "a"}, {@code "b"}, or {@code "c"}.
     *   The hyphen ({@code -}) may be used to specify a range so {@code [a-z]}
     *   specifies a range that matches from {@code "a"} to {@code "z"} (inclusive).
     *   These forms can be mixed so [abce-g] matches {@code "a"}, {@code "b"},
     *   {@code "c"}, {@code "e"}, {@code "f"} or {@code "g"}. If the character
     *   after the {@code [} is a {@code !} then it is used for negation so {@code
     *   [!a-c]} matches any character except {@code "a"}, {@code "b"}, or {@code
     *   "c"}.
     *   <p> Within a bracket expression the {@code *}, {@code ?} and {@code \}
     *   characters match themselves. The ({@code -}) character matches itself if
     *   it is the first character within the brackets, or the first character
     *   after the {@code !} if negating.</p></li>
     *
     *   <li><p> The {@code { }} characters are a group of subpatterns, where
     *   the group matches if any subpattern in the group matches. The {@code ","}
     *   character is used to separate the subpatterns. Groups cannot be nested.
     *   </p></li>
     *
     *   <li><p>All other characters match themselves in an implementation
     *   dependent manner. </p></li>
     * </ul>
     *
     * <p> When the syntax is "{@code regex}" then the pattern component is a
     * regular expression as defined by the {@link java.util.regex.Pattern}
     * class.
     *
     * <p> For both the glob and regex syntaxes then whether matching is case
     * sensitive, and other matching details, are implementation-dependent.
     *
     * <p> The {@code PathMatcher} returned by this method matches on the {@link
     * Path#getName name} component of a {@code Path}. If the {@link
     * PathMatcher#matches matches} method is invoked with a {@code Path} that
     * does not have a {@code name} component then it returns {@code false}.
     *
     * @param   syntax
     *          The pattern syntax
     * @param   pattern
     *          The pattern
     *
     * @return  A file name matcher that matches file names against the pattern
     *
     * @throws  java.util.regex.PatternSyntaxException
     *          If the pattern is invalid
     * @throws  UnsupportedOperationException
     *          If the pattern syntax is not known to the implementation
     *
     * @see Path#newDirectoryStream(String)
     */
    public abstract PathMatcher getNameMatcher(String syntax, String pattern);

    /**
     * Returns the {@code UserPrincipalLookupService} for this file system
     * <i>(optional operation)</i>. The resulting lookup service may be used to
     * lookup user or group names.
     *
     * <p> <b>Usage Example:</b>
     * Suppose we want to make "joe" the owner of a file:
     * <pre>
     *     Path file = ...
     *     UserPrincipal joe = file.getFileSystem().getUserPrincipalLookupService()
     *         .lookupPrincipalByName("joe");
     *     Attributes.setOwner(file, joe);
     * </pre>
     *
     * @throws  UnsupportedOperationException
     *          If this {@code FileSystem} does not does have a lookup service
     *
     * @return  The {@code UserPrincipalLookupService} for this file system
     */
    public abstract UserPrincipalLookupService getUserPrincipalLookupService();

    /**
     * Constructs a new {@link WatchService} <i>(optional operation)</i>.
     *
     * <p> This method constructs a new watch service that may be used to watch
     * registered objects for changes and events.
     *
     * @return  a new watch service
     *
     * @throws  UnsupportedOperationException
     *          If this {@code FileSystem} does not support watching file system
     *          objects for changes and events. This exception is not thrown
     *          by {@code FileSystems} created by the default provider.
     * @throws  IOException
     *          If an I/O error occurs
     */
    public abstract WatchService newWatchService() throws IOException;
}
