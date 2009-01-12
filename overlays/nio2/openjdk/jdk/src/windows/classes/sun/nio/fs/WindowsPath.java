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

package sun.nio.fs;

import java.nio.file.*;
import java.nio.file.attribute.*;
import java.nio.file.spi.AbstractPath;
import java.nio.channels.*;
import java.io.*;
import java.net.URI;
import java.security.AccessController;
import java.util.*;

import com.sun.nio.file.ExtendedWatchEventModifier;

import sun.security.util.SecurityConstants;
import sun.misc.Unsafe;

import static sun.nio.fs.WindowsNativeDispatcher.*;
import static sun.nio.fs.WindowsConstants.*;

/**
 * Windows implementation of Path
 */

class WindowsPath extends AbstractPath {
    private static final Unsafe unsafe = Unsafe.getUnsafe();

    // FIXME - eliminate this reference to reduce space
    private final WindowsFileSystem fs;

    // path type
    private final WindowsPathType type;
    // root component (may be empty)
    private final String root;
    // normalized path
    private final String path;

    // offsets into name components (computed lazily)
    private volatile Integer[] offsets;

    // resolved against file system's default directory
    private final String resolved;

    // the path to use in calls to Windows (same as resolved path when less
    // than 248 characters but differs for long paths)
    private final String pathForWin32Calls;

    // computed hash code (computed lazily, no need to be volatile)
    private volatile int hash;


    /**
     * Initializes a new instance of this class.
     */
    WindowsPath(WindowsFileSystem fs,
                WindowsPathType type,
                String root,
                String path)
    {
        this.fs = fs;
        this.type = type;
        this.root = root;
        this.path = path;

        // resolve path against file system's default directory
        switch (type) {
            case ABSOLUTE :
            case UNC :
                this.resolved = path;
                break;
            case RELATIVE :
                String defaultDirectory = getFileSystem().defaultDirectory();
                if (defaultDirectory.endsWith("\\")) {
                    this.resolved = defaultDirectory + path;
                } else {
                    this.resolved = defaultDirectory + "\\" + path;
                }
                break;
            case DIRECTORY_RELATIVE:
                String defaultRoot = getFileSystem().defaultRoot();
                this.resolved = defaultRoot + path.substring(1);
                break;
            case DRIVE_RELATIVE:
                // Need to postpone resolving these types of paths as they can
                // change for the case of removable media devices when media
                // is inserted or removed.
                this.resolved = path;
                break;
            default:
                throw new AssertionError();
        }

        // Long paths need to have "." and ".." removed and be prefixed with \\?\
        // Note that it is okay to remove ".." even when it follows a link -
        // for example, it is okay for foo/link/../bar to be changed to foo/bar.
        // The reason is that Win32 APIs to access foo/link/../bar will access
        // foo/bar anyway (which differs to Unix systems)
        String ps = resolved;
        if (ps.length() > 248) {
            if (ps.length() > 32000) {
                throw new InvalidPathException(ps,
                    "Cannot access file with path exceeding 32000 characters");
            }
            try {
                ps = GetFullPathName(ps);
            } catch (WindowsException x) {
                throw new AssertionError("GetFullPathName failed for '" + ps +
                    "' " + x.getMessage());
            }
            ps = addPrefixIfNeeded(ps);
        }

        this.pathForWin32Calls = ps;
    }

    /**
     * Creates a WindowsPath by parsing the given path.
     */
    static WindowsPath parse(WindowsFileSystem fs, String path) {
        WindowsPathParser.Result result = WindowsPathParser.parse(path);
        return new WindowsPath(fs, result.type(), result.root(), result.path());
    }

    /**
     * Creates a WindowsPath from a given path that is known to be normalized.
     */
    static WindowsPath createFromNormalizedPath(WindowsFileSystem fs, String path) {
        try {
            WindowsPathParser.Result result =
                WindowsPathParser.parseNormalizedPath(path);
            return new WindowsPath(fs, result.type(), result.root(), result.path());
        } catch (InvalidPathException x) {
            throw new AssertionError(x.getMessage());
        }
    }

    // use this message when throwing exceptions
    String getPathForExceptionMessage() {
        return path;
    }

    // use this path for permission checks
    String getPathForPermissionCheck() {
        return resolved;
    }

    // use this for calls into Windows (may have \\?\ or \\?\UNC prefix)
    String getPathForWin32Calls() {
        return pathForWin32Calls;
    }

    // Add long path prefix to path if required
    private static String addPrefixIfNeeded(String path) {
        if (path.length() > 248) {
            if (path.startsWith("\\\\")) {
                path = "\\\\?\\UNC" + path.substring(1, path.length());
            } else {
                path = "\\\\?\\" + path;
            }
        }
        return path;
    }


    public WindowsFileSystem getFileSystem() {
        return fs;
    }

    // -- Path operations --


    public Path getName() {
        // represents root component only
        if (root.length() == path.length())
            return null;
        int off = path.lastIndexOf('\\');
        if (off < root.length())
            off = root.length();
        else
            off++;
        return new WindowsPath(getFileSystem(), WindowsPathType.RELATIVE, "", path.substring(off));
    }


    public WindowsPath getParent() {
        // represents root component only
        if (root.length() == path.length())
            return null;
        int off = path.lastIndexOf('\\');
        if (off < root.length())
            return getRoot();
        else
            return new WindowsPath(getFileSystem(),
                                   type,
                                   root,
                                   path.substring(0, off));
    }


    public WindowsPath getRoot() {
        if (root.length() == 0)
            return null;
        return new WindowsPath(getFileSystem(), type, root, root);
    }

    // package-private
    boolean isUnc() {
        return type == WindowsPathType.UNC;
    }


    public boolean isAbsolute() {
        return type == WindowsPathType.ABSOLUTE || type == WindowsPathType.UNC;
    }

    private WindowsPath checkPath(FileRef path) {
        if (path == null)
            throw new NullPointerException();
        if (!(path instanceof WindowsPath)) {
            throw new ProviderMismatchException();
        }
        return (WindowsPath)path;
    }


    public WindowsPath relativize(Path obj) {
        WindowsPath other = checkPath(obj);
        if (this.equals(other))
            return null;

        // can only relativize paths of the same type
        if (this.type != other.type)
            throw new IllegalArgumentException("'other' is different type of Path");

        // can only relativize paths if root component matches
        if (!this.root.equalsIgnoreCase(other.root))
            throw new IllegalArgumentException("'other' has different root");

        int bn = this.getNameCount();
        int cn = other.getNameCount();

        // skip matching names
        int n = (bn > cn) ? cn : bn;
        int i = 0;
        while (i < n) {
            if (!this.getName(i).equals(other.getName(i)))
                break;
            i++;
        }

        // append ..\ for remaining names in the base
        StringBuilder result = new StringBuilder();
        for (int j=i; j<bn; j++) {
            result.append("..\\");
        }

        // append remaining names in child
        for (int j=i; j<cn; j++) {
            result.append(other.getName(j).toString());
            result.append("\\");
        }

        // drop trailing slash in result
        result.setLength(result.length()-1);
        return createFromNormalizedPath(getFileSystem(), result.toString());
    }


    public Path normalize() {
        final int count = getNameCount();
        if (count == 0)
            return this;

        boolean[] ignore = new boolean[count];      // true => ignore name
        int remaining = count;                      // number of names remaining

        // multiple passes to eliminate all occurences of "." and "name/.."
        int prevRemaining;
        do {
            prevRemaining = remaining;
            int prevName = -1;
            for (int i=0; i<count; i++) {
                if (ignore[i])
                    continue;

                String name = elementAsString(i);

                // not "." or ".."
                if (name.length() > 2) {
                    prevName = i;
                    continue;
                }

                // "." or something else
                if (name.length() == 1) {
                    // ignore "."
                    if (name.charAt(0) == '.') {
                        ignore[i] = true;
                        remaining--;
                    } else {
                        prevName = i;
                    }
                    continue;
                }

                // not ".."
                if (name.charAt(0) != '.' || name.charAt(1) != '.') {
                    prevName = i;
                    continue;
                }

                // ".." found
                if (prevName >= 0) {
                    // name/<ignored>/.. found so mark name and ".." to be
                    // ignored
                    ignore[prevName] = true;
                    ignore[i] = true;
                    remaining = remaining - 2;
                    prevName = -1;
                } else {
                    // Cases:
                    //    C:\<ignored>\..
                    //    \\server\\share\<ignored>\..
                    //    \<ignored>..
                    if (isAbsolute() || type == WindowsPathType.DIRECTORY_RELATIVE) {
                        boolean hasPrevious = false;
                        for (int j=0; j<i; j++) {
                            if (!ignore[j]) {
                                hasPrevious = true;
                                break;
                            }
                        }
                        if (!hasPrevious) {
                            // all proceeding names are ignored
                            ignore[i] = true;
                            remaining--;
                        }
                    }
                }
            }
        } while (prevRemaining > remaining);

        // no redundant names
        if (remaining == count)
            return this;

        // corner case - all names removed
        if (remaining == 0) {
            return getRoot();
        }

        // re-constitute the path from the remaining names.
        StringBuilder result = new StringBuilder();
        if (root != null)
            result.append(root);
        for (int i=0; i<count; i++) {
            if (!ignore[i]) {
                result.append(getName(i).toString());
                result.append("\\");
            }
        }

        // drop trailing slash in result
        result.setLength(result.length()-1);
        return createFromNormalizedPath(getFileSystem(), result.toString());
    }


    public WindowsPath resolve(Path obj) {
        if (obj == null)
            return this;
        WindowsPath other = checkPath(obj);
        if (other.isAbsolute())
            return other;

        switch (other.type) {
            case RELATIVE: {
                String result;
                if (path.endsWith("\\") || (root.length() == path.length())) {
                    result = path + other.path;
                } else {
                    result = path + "\\" + other.path;
                }
                return new WindowsPath(getFileSystem(), type, root, result);
            }

            case DIRECTORY_RELATIVE: {
                String result;
                if (root.endsWith("\\")) {
                    result = root + other.path.substring(1);
                } else {
                    result = root + other.path;
                }
                return createFromNormalizedPath(getFileSystem(), result);
            }

            case DRIVE_RELATIVE: {
                if (!root.endsWith("\\"))
                    return other;
                // if different roots then return other
                String thisRoot = root.substring(0, root.length()-1);
                if (!thisRoot.equalsIgnoreCase(other.root))
                    return other;
                // same roots
                String remaining = other.path.substring(other.root.length());
                String result;
                if (path.endsWith("\\")) {
                    result = path + remaining;
                } else {
                    result = path + "\\" + remaining;
                }
                return createFromNormalizedPath(getFileSystem(), result);
            }

            default:
                throw new AssertionError();
        }
    }


    public WindowsPath resolve(String other) {
        return resolve(getFileSystem().getPath(other));
    }

    // generate offset array
    private void initOffsets() {
        if (offsets == null) {
            ArrayList<Integer> list = new ArrayList<Integer>();
            int start = root.length();
            int off = root.length();
            while (off < path.length()) {
                if (path.charAt(off) != '\\') {
                    off++;
                } else {
                    list.add(start);
                    start = ++off;
                }
            }
            if (start != off)
                list.add(start);
            synchronized (this) {
                if (offsets == null)
                    offsets = list.toArray(new Integer[list.size()]);
            }
        }
    }


    public int getNameCount() {
        initOffsets();
        return offsets.length;
    }

    private String elementAsString(int i) {
        initOffsets();
        if (i == (offsets.length-1))
            return path.substring(offsets[i]);
        return path.substring(offsets[i], offsets[i+1]-1);
    }


    public WindowsPath getName(int index) {
        initOffsets();
        if (index < 0 || index >= offsets.length)
            throw new IllegalArgumentException();
        return new WindowsPath(getFileSystem(), WindowsPathType.RELATIVE, "", elementAsString(index));
    }


    public WindowsPath subpath(int beginIndex, int endIndex) {
        initOffsets();
        if (beginIndex < 0)
            throw new IllegalArgumentException();
        if (beginIndex >= offsets.length)
            throw new IllegalArgumentException();
        if (endIndex > offsets.length)
            throw new IllegalArgumentException();
        if (beginIndex >= endIndex)
            throw new IllegalArgumentException();

        StringBuilder sb = new StringBuilder();
        Integer[] nelems = new Integer[endIndex - beginIndex];
        for (int i = beginIndex; i < endIndex; i++) {
            nelems[i-beginIndex] = sb.length();
            sb.append(elementAsString(i));
            if (i != (endIndex-1))
                sb.append("\\");
        }
        return new WindowsPath(getFileSystem(), WindowsPathType.RELATIVE, "", sb.toString());
    }


    public boolean startsWith(Path obj) {
        WindowsPath other = checkPath(obj);

        // if this path has a root component the given path's root must match
        if (!this.root.equalsIgnoreCase(other.root))
            return false;

        // roots match so compare elements
        int thisCount = getNameCount();
        int otherCount = other.getNameCount();
        if (otherCount <= thisCount) {
            while (--otherCount >= 0) {
                String thisElement = this.elementAsString(otherCount);
                String otherElement = other.elementAsString(otherCount);
                // FIXME: should compare in uppercase
                if (!thisElement.equalsIgnoreCase(otherElement))
                    return false;
            }
            return true;
        }
        return false;
    }


    public boolean endsWith(Path obj) {
        WindowsPath other = checkPath(obj);

        // other path is longer
        if (other.path.length() > path.length()) {
            return false;
        }

        int thisCount = this.getNameCount();
        int otherCount = other.getNameCount();

        // given path has more elements that this path
        if (otherCount > thisCount) {
            return false;
        }

        // compare roots
        if (other.root.length() > 0) {
            if (otherCount < thisCount)
                return false;
            // FIXME: should compare in uppercase
            if (!this.root.equalsIgnoreCase(other.root))
                return false;
        }

        // match last 'otherCount' elements
        int off = thisCount - otherCount;
        while (--otherCount >= 0) {
            String thisElement = this.elementAsString(off + otherCount);
            String otherElement = other.elementAsString(otherCount);
            // FIXME: should compare in uppercase
            if (!thisElement.equalsIgnoreCase(otherElement))
                return false;
        }
        return true;
    }


    public int compareTo(Path obj) {
        if (obj == null)
            throw new NullPointerException();
        String s1 = path;
        String s2 = ((WindowsPath)obj).path;
        int n1 = s1.length();
        int n2 = s2.length();
        int min = Math.min(n1, n2);
        for (int i = 0; i < min; i++) {
            char c1 = s1.charAt(i);
            char c2 = s2.charAt(i);
             if (c1 != c2) {
                 c1 = Character.toUpperCase(c1);
                 c2 = Character.toUpperCase(c2);
                 if (c1 != c2) {
                     return c1 - c2;
                 }
             }
        }
        return n1 - n2;
    }


    public boolean equals(Object obj) {
        if ((obj != null) && (obj instanceof WindowsPath)) {
            return compareTo((Path)obj) == 0;
        }
        return false;
    }


    public int hashCode() {
        // OK if two or more threads compute hash
        int h = hash;
        if (h == 0) {
            for (int i = 0; i< path.length(); i++) {
                h = 31*h + Character.toUpperCase(path.charAt(i));
            }
            hash = h;
        }
        return h;
    }


    public String toString() {
        return path;
    }


    public Iterator<Path> iterator() {
        return new Iterator<Path>() {
            private int i = 0;

            public boolean hasNext() {
                return (i < getNameCount());
            }

            public Path next() {
                if (i < getNameCount()) {
                    Path result = getName(i);
                    i++;
                    return result;
                } else {
                    throw new NoSuchElementException();
                }
            }

            public void remove() {
                throw new UnsupportedOperationException();
            }
        };
    }

    // -- file operations --

    // package-private
    long openForReadAttributeAccess(boolean followLinks)
        throws WindowsException
    {
        int flags = FILE_FLAG_BACKUP_SEMANTICS;
        if (!followLinks && getFileSystem().supportsLinks())
            flags |= FILE_FLAG_OPEN_REPARSE_POINT;
        return CreateFile(getPathForWin32Calls(),
                          FILE_READ_ATTRIBUTES,
                          (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
                          0L,
                          OPEN_EXISTING,
                          flags);
    }

    void checkRead() {
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkRead(getPathForPermissionCheck());
        }
    }

    void checkWrite() {
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkWrite(getPathForPermissionCheck());
        }
    }

    void checkDelete() {
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkDelete(getPathForPermissionCheck());
        }
    }


    public FileStore getFileStore()
        throws IOException
    {
        checkRead();
        return WindowsFileStore.create(this);
    }

    /**
     * Returns buffer with SID_AND_ATTRIBUTES structure representing the user
     * associated with the current thread access token.
     * FIXME - this should be cached.
     */
    private NativeBuffer getUserInfo() throws IOException {
        try {
            long hToken = WindowsSecurity.processTokenWithQueryAccess;
            int size = GetTokenInformation(hToken, TokenUser, 0L, 0);
            assert size > 0;

            NativeBuffer buffer = NativeBuffers.getNativeBuffer(size);
            try {
                int newsize = GetTokenInformation(hToken, TokenUser,
                                                  buffer.address(), size);
                if (newsize != size)
                    throw new AssertionError();
                return buffer;
            } catch (WindowsException x) {
                buffer.release();
                throw x;
            }
        } catch (WindowsException x) {
            throw new IOException(x.getMessage());
        }
    }

    /**
     * Reads the file ACL and return the effective access as ACCESS_MASK
     */
    private int getEffectiveAccess() throws IOException {
        // read security descriptor continaing ACL (symlinks are followed)
        String fp = WindowsLinkSupport.getFinalPath(this);
        NativeBuffer aclBuffer = WindowsAclFileAttributeView
            .getFileSecurity(fp, DACL_SECURITY_INFORMATION);

        // retrieves DACL from security descriptor
        long pAcl = GetSecurityDescriptorDacl(aclBuffer.address());

        // Use GetEffectiveRightsFromAcl to get effective access to file
        try {
            NativeBuffer userBuffer = getUserInfo();
            try {
                try {
                    // SID_AND_ATTRIBUTES->pSid
                    long pSid = unsafe.getAddress(userBuffer.address());
                    long pTrustee = BuildTrusteeWithSid(pSid);
                    try {
                        return GetEffectiveRightsFromAcl(pAcl, pTrustee);
                    } finally {
                        LocalFree(pTrustee);
                    }
                } catch (WindowsException x) {
                    throw new IOException("Unable to get effective rights from ACL: " +
                        x.getMessage());
                }
            } finally {
                userBuffer.release();
            }
        } finally {
            aclBuffer.release();
        }
    }


    public void checkAccess(AccessMode... modes) throws IOException {
        // if no access modes then simply file attributes
        if (modes.length == 0) {
            checkRead();
            try {
                WindowsFileAttributes.get(this, true);
            } catch (WindowsException exc) {
                exc.rethrowAsIOException(this);
            }
            return;
        }

        boolean r = false;
        boolean w = false;
        boolean x = false;
        for (AccessMode mode: modes) {
            switch (mode) {
                case READ : r = true; break;
                case WRITE : w = true; break;
                case EXECUTE : x = true; break;
                default: throw new AssertionError("Should not get here");
            }
        }

        int mask = 0;
        if (r) {
            checkRead();
            mask |= FILE_READ_DATA;
        }
        if (w) {
            checkWrite();
            mask |= FILE_WRITE_DATA;
        }
        if (x) {
            SecurityManager sm = System.getSecurityManager();
            if (sm != null)
                sm.checkExec(getPathForPermissionCheck());
            mask |= FILE_EXECUTE;
        }

        if ((getEffectiveAccess() & mask) == 0)
            throw new AccessDeniedException(
                this.getPathForExceptionMessage(), null,
                "Effective permissions does not allow requested access");

        // for write access we neeed to check if the DOS readonly attribute
        // and if the volume is read-only
        if (w) {
            try {
                WindowsFileAttributes attrs = WindowsFileAttributes.get(this, true);
                if (!attrs.isDirectory() && attrs.isReadOnly())
                    throw new AccessDeniedException(
                        this.getPathForExceptionMessage(), null,
                        "DOS readonly attribute is set");
            } catch (WindowsException exc) {
                exc.rethrowAsIOException(this);
            }

            if (WindowsFileStore.create(this).isReadOnly()) {
                throw new AccessDeniedException(
                    this.getPathForExceptionMessage(), null, "Read-only file system");
            }
            return;
        }
    }


    public void delete(boolean failIfNotExists) throws IOException {
        checkDelete();

        WindowsFileAttributes attrs = null;
        try {
             // need to know if file is a directory or junction
             attrs = WindowsFileAttributes.get(this, false);
             if (attrs.isDirectory() || attrs.isDirectoryLink()) {
                RemoveDirectory(getPathForWin32Calls());
             } else {
                DeleteFile(getPathForWin32Calls());
             }
        } catch (WindowsException x) {

            // no-op if file does not exist
            if (!failIfNotExists &&
                (x.lastError() == ERROR_FILE_NOT_FOUND ||
                 x.lastError() == ERROR_PATH_NOT_FOUND)) return;

            if (attrs != null && attrs.isDirectory()) {
                // ERROR_ALREADY_EXISTS is returned when attempting to delete
                // non-empty directory on SAMBA servers.
                if (x.lastError() == ERROR_DIR_NOT_EMPTY ||
                    x.lastError() == ERROR_ALREADY_EXISTS)
                {
                    throw new DirectoryNotEmptyException(
                        getPathForExceptionMessage());
                }
            }
            x.rethrowAsIOException(this);
        }
    }


    public DirectoryStream<Path> newDirectoryStream(DirectoryStream.Filter<? super Path> filter)
        throws IOException
    {
        checkRead();
        if (filter == null)
            throw new NullPointerException();
        return new WindowsDirectoryStream(this, filter);
    }


    public void implCopyTo(Path obj, CopyOption... options) throws IOException {
        WindowsPath target = (WindowsPath)obj;
        WindowsFileCopy.copy(this, target, options);
    }


    public void implMoveTo(Path obj, CopyOption... options) throws IOException {
        WindowsPath target = (WindowsPath)obj;
        WindowsFileCopy.move(this, target, options);
    }

    private boolean followLinks(LinkOption... options) {
        boolean followLinks = true;
        for (LinkOption option: options) {
            if (option == LinkOption.NOFOLLOW_LINKS) {
                followLinks = false;
                continue;
            }
            throw new AssertionError("Should not get here");
        }
        return followLinks;
    }


    @SuppressWarnings("unchecked")
    public <V extends FileAttributeView> V
        getFileAttributeView(Class<V> view, LinkOption... options)
    {
        if (view == null)
            throw new NullPointerException();
        boolean followLinks = followLinks(options);
        if (view == BasicFileAttributeView.class)
            return (V) WindowsFileAttributeViews.createBasicView(this, followLinks);
        if (view == DosFileAttributeView.class)
            return (V) WindowsFileAttributeViews.createDosView(this, followLinks);
        if (view == AclFileAttributeView.class)
            return (V) new WindowsAclFileAttributeView(this, followLinks);
        if (view == FileOwnerAttributeView.class)
            return (V) new FileOwnerAttributeViewImpl(
                new WindowsAclFileAttributeView(this, followLinks));
        if (view == NamedAttributeView.class)
            return (V) new WindowsNamedAttributeView(this, followLinks);
        return (V) null;
    }


    public FileAttributeView getFileAttributeView(String name, LinkOption... options) {
        boolean followLinks = followLinks(options);
        if (name.equals("basic"))
            return WindowsFileAttributeViews.createBasicView(this, followLinks);
        if (name.equals("dos"))
            return WindowsFileAttributeViews.createDosView(this, followLinks);
        if (name.equals("acl"))
            return new WindowsAclFileAttributeView(this, followLinks);
        if (name.equals("owner"))
            return new FileOwnerAttributeViewImpl(
                new WindowsAclFileAttributeView(this, followLinks));
        if (name.equals("xattr"))
            return new WindowsNamedAttributeView(this, followLinks);
        return null;
    }


    public WindowsPath createDirectory(FileAttribute<?>... attrs)
        throws IOException
    {
        checkWrite();
        WindowsSecurityDescriptor sd = WindowsSecurityDescriptor.fromAttribute(attrs);
        try {
            CreateDirectory(getPathForWin32Calls(), sd.address());
        } catch (WindowsException x) {
            x.rethrowAsIOException(this);
        } finally {
            sd.release();
        }
        return this;
    }


    public InputStream newInputStream()throws IOException {
        try {
            Set<OpenOption> options = Collections.emptySet();
            FileChannel fc = WindowsChannelFactory
                .newFileChannel(getPathForWin32Calls(),
                                getPathForPermissionCheck(),
                                options,
                                0L);
            return Channels.newInputStream(fc);
        } catch (WindowsException x) {
            x.rethrowAsIOException(this);
            return null;  // keep compiler happy
        }
    }


    public SeekableByteChannel newByteChannel(Set<? extends OpenOption> options,
                                              FileAttribute<?>... attrs)
         throws IOException
    {
        WindowsSecurityDescriptor sd =
            WindowsSecurityDescriptor.fromAttribute(attrs);
        try {
            return WindowsChannelFactory
                .newFileChannel(getPathForWin32Calls(),
                                getPathForPermissionCheck(),
                                options,
                                sd.address());
        } catch (WindowsException x) {
            x.rethrowAsIOException(this);
            return null;  // keep compiler happy
        } finally {
            sd.release();
        }
    }


    public OutputStream newOutputStream(Set<? extends OpenOption> options,
                                        FileAttribute<?>... attrs)
        throws IOException
    {
        // need to copy options to add WRITE
        Set<OpenOption> opts = new HashSet<OpenOption>(options);
        if (opts.contains(StandardOpenOption.READ))
            throw new IllegalArgumentException("READ not allowed");
        opts.add(StandardOpenOption.WRITE);

        WindowsSecurityDescriptor sd =
            WindowsSecurityDescriptor.fromAttribute(attrs);
        FileChannel fc;
        try {
            fc = WindowsChannelFactory
                .newFileChannel(getPathForWin32Calls(),
                                getPathForPermissionCheck(),
                                opts,
                                sd.address());
            return Channels.newOutputStream(fc);
        } catch (WindowsException x) {
            x.rethrowAsIOException(this);
            return null;  // keep compiler happy
        } finally {
            sd.release();
        }
    }


    public boolean isSameFile(FileRef obj) throws IOException {
        if (this.equals(obj))
            return true;
        if (!(obj instanceof WindowsPath))  // includes null check
            return false;
        WindowsPath other = (WindowsPath)obj;

        // check security manager access to both files
        this.checkRead();
        other.checkRead();

        // open both files and see if they are the same
        long h1 = 0L;
        try {
            h1 = this.openForReadAttributeAccess(true);
        } catch (WindowsException x) {
            x.rethrowAsIOException(this);
        }
        try {
            WindowsFileAttributes attrs1 = null;
            try {
                attrs1 = WindowsFileAttributes.readAttributes(h1);
            } catch (WindowsException x) {
                x.rethrowAsIOException(this);
            }
            long h2 = 0L;
            try {
                h2 = other.openForReadAttributeAccess(true);
            } catch (WindowsException x) {
                x.rethrowAsIOException(other);
            }
            try {
                WindowsFileAttributes attrs2 = null;
                try {
                    attrs2 = WindowsFileAttributes.readAttributes(h2);
                } catch (WindowsException x) {
                    x.rethrowAsIOException(other);
                }
                return WindowsFileAttributes.isSameFile(attrs1, attrs2);
            } finally {
                CloseHandle(h2);
            }
        } finally {
            CloseHandle(h1);
        }
    }


    public WindowsPath createSymbolicLink(Path obj, FileAttribute<?>... attrs)
        throws IOException
    {
        if (!getFileSystem().supportsLinks()) {
            throw new UnsupportedOperationException("Symbolic links not supported "
                + "on this operating system");
        }

        WindowsPath target = checkPath(obj);

        // no attributes allowed
        if (attrs.length > 0) {
            if (attrs[0] == null)
                throw new NullPointerException();
            throw new ProviderMismatchException();
        }

        // permission check
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkPermission(new LinkPermission("symbolic"));
            this.checkWrite();
        }

        /**
         * Throw I/O exception for the drive-relative case because Windows
         * creates a link with the resolved target for this case.
         */
        if (target.type == WindowsPathType.DRIVE_RELATIVE) {
            throw new IOException("Cannot create symbolic link to drive-relative target");
        }

        /*
         * Windows treates symbolic links to directories differently than it
         * does to other file types. For that reason we check if the exists and
         * is a directory.
         */
        int flags = 0;
        WindowsPath resolvedTarget =
            WindowsPath.createFromNormalizedPath(getFileSystem(), resolve(target).path);
        try {
            if (WindowsFileAttributes.get(resolvedTarget, true).isDirectory())
                flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
        } catch (WindowsException x) {
            // unable to access target so assume target is not a directory
        }

        // create the link
        try {
            CreateSymbolicLink(getPathForWin32Calls(),
                               addPrefixIfNeeded(target.toString()),
                               flags);
        } catch (WindowsException x) {
            if (x.lastError() == ERROR_INVALID_REPARSE_DATA) {
                x.rethrowAsIOException(this, target);
            } else {
                x.rethrowAsIOException(this);
            }
        }
        return this;
    }


    public Path createLink(Path obj) throws IOException {
        WindowsPath existing = checkPath(obj);

        // permission check
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkPermission(new LinkPermission("hard"));
            this.checkWrite();
            existing.checkWrite();
        }

        // create hard link
        try {
            CreateHardLink(this.getPathForWin32Calls(),
                           existing.getPathForWin32Calls());
        } catch (WindowsException x) {
            x.rethrowAsIOException(this, existing);
        }

        return this;
    }


    public WindowsPath readSymbolicLink() throws IOException {
        if (!getFileSystem().supportsLinks()) {
            throw new UnsupportedOperationException("symbolic links not supported");
        }

        // permission check
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            FilePermission perm = new FilePermission(getPathForPermissionCheck(),
                SecurityConstants.FILE_READLINK_ACTION);
            AccessController.checkPermission(perm);
        }

        String target = WindowsLinkSupport.readLink(this);
        return createFromNormalizedPath(getFileSystem(), target);
    }


    public URI toUri() {
        return WindowsUriSupport.toUri(this);
    }

    // returns true if same drive letter
    private static boolean isSameDrive(String root1, String root2) {
        return Character.toUpperCase(root1.charAt(0)) ==
               Character.toUpperCase(root2.charAt(0));
    }


    public WindowsPath toAbsolutePath() {
        if (isAbsolute())
            return this;

        // permission check as per spec
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkPropertyAccess("user.dir");
        }

        // result is the resolved path if the path is not drive-relative
        if (type != WindowsPathType.DRIVE_RELATIVE)
            return WindowsPath.createFromNormalizedPath(getFileSystem(), resolved);

        // if the path is drive-relative and is relative to a drive other
        // than the drive of the default directory then resolve against the
        // working directory of that drive.
        if (!isSameDrive(root, getFileSystem().defaultRoot())) {
            try {
                int dt = GetDriveType(root + "\\");
                if (dt == DRIVE_UNKNOWN || dt == DRIVE_NO_ROOT_DIR)
                    throw new WindowsException("");
                String wd = GetFullPathName(root + ".");
                String result = wd;
                if (wd.endsWith("\\")) {
                    result += path.substring(root.length());
                } else {
                    if (path.length() > root.length())
                        result += "\\" + path.substring(root.length());
                }
                return WindowsPath.createFromNormalizedPath(getFileSystem(), result);
            } catch (WindowsException x) {
                IOException ioe = new IOException("Unable to get working " +
                    "directory of drive '" +
                    Character.toUpperCase(root.charAt(0)) + "'");
                throw new java.io.IOError(ioe);
            }
        } else {
            // relative to default directory
            String remaining = path.substring(root.length());
            String defaultDirectory = getFileSystem().defaultDirectory();
            String result;
            if (defaultDirectory.endsWith("\\")) {
                result = defaultDirectory + remaining;
            } else {
                result = defaultDirectory + "\\" + remaining;
            }
            return createFromNormalizedPath(getFileSystem(), result);
        }
    }


    public WindowsPath toRealPath(boolean resolveLinks) throws IOException {
        checkRead();
        String rp = WindowsLinkSupport.getRealPath(this, resolveLinks);
        return createFromNormalizedPath(getFileSystem(), rp);
    }


    public boolean isHidden() throws IOException {
        checkRead();
        WindowsFileAttributes attrs = null;
        try {
            attrs = WindowsFileAttributes.get(this, true);
        } catch (WindowsException x) {
            x.rethrowAsIOException(this);
        }
        // DOS hidden attribute not meaningful when set on directories
        if (attrs.isDirectory())
            return false;
        return attrs.isHidden();
    }


    public WatchKey register(WatchService watcher,
                             WatchEvent.Kind<?>[] events,
                             WatchEvent.Modifier... modifiers)
        throws IOException
    {
        if (watcher == null)
            throw new NullPointerException();
        if (!(watcher instanceof WindowsWatchService))
            throw new ProviderMismatchException();

        // When a security manager is set then we need to make a defensive
        // copy of the modifiers and check for the Windows specific FILE_TREE
        // modifier. When the modifier is present then check that permission
        // has been granted recursively.
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            boolean watchSubtree = false;
            int ml = modifiers.length;
            if (ml > 0) {
                WatchEvent.Modifier[] copy = new WatchEvent.Modifier[ml];
                System.arraycopy(modifiers, 0, copy, 0, ml);
                modifiers = copy;
                int i=0;
                while (i < ml) {
                    if (modifiers[i++] == ExtendedWatchEventModifier.FILE_TREE) {
                        watchSubtree = true;
                        break;
                    }
                }
            }
            String s = getPathForPermissionCheck();
            sm.checkRead(s);
            if (watchSubtree)
                sm.checkRead(s + "\\-");
        }

        return ((WindowsWatchService)watcher).register(this, events, modifiers);
    }
}
