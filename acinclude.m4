AC_DEFUN([SET_ARCH_DIRS],
[
  case "${host}" in
    x86_64-*-*)
      BUILD_ARCH_DIR=amd64
      INSTALL_ARCH_DIR=amd64
      JRE_ARCH_DIR=amd64
      ;;
    i?86-*-*)
      BUILD_ARCH_DIR=i586
      INSTALL_ARCH_DIR=i386
      JRE_ARCH_DIR=i386
      ARCH_PREFIX=${LINUX32}
      ;;
    alpha*-*-*)
      BUILD_ARCH_DIR=alpha
      INSTALL_ARCH_DIR=alpha
      JRE_ARCH_DIR=alpha
      ;;
    arm*-*-*)
      BUILD_ARCH_DIR=arm
      INSTALL_ARCH_DIR=arm
      JRE_ARCH_DIR=arm
      ;;
    mips-*-*)
      BUILD_ARCH_DIR=mips
      INSTALL_ARCH_DIR=mips
      JRE_ARCH_DIR=mips
       ;;
    mipsel-*-*)
      BUILD_ARCH_DIR=mipsel
      INSTALL_ARCH_DIR=mipsel
      JRE_ARCH_DIR=mipsel
       ;;
    powerpc-*-*)
      BUILD_ARCH_DIR=ppc
      INSTALL_ARCH_DIR=ppc
      JRE_ARCH_DIR=ppc
      ARCH_PREFIX=${LINUX32}
       ;;
    powerpc64-*-*)
      BUILD_ARCH_DIR=ppc64
      INSTALL_ARCH_DIR=ppc64
      JRE_ARCH_DIR=ppc64
       ;;
    sparc64-*-*)
      BUILD_ARCH_DIR=sparcv9
      INSTALL_ARCH_DIR=sparcv9
      JRE_ARCH_DIR=sparc64
       ;;
    s390-*-*)
      BUILD_ARCH_DIR=s390
      INSTALL_ARCH_DIR=s390
      JRE_ARCH_DIR=s390
      ARCH_PREFIX=${LINUX32}
       ;;
    *)
      BUILD_ARCH_DIR=`uname -m`
      INSTALL_ARCH_DIR=$BUILD_ARCH_DIR
      JRE_ARCH_DIR=$INSTALL_ARCH_DIR
      ;;
  esac
  AC_SUBST(BUILD_ARCH_DIR)
  AC_SUBST(INSTALL_ARCH_DIR)
  AC_SUBST(JRE_ARCH_DIR)
  AC_SUBST(ARCH_PREFIX)
])

AC_DEFUN([FIND_JAVAC],
[
  user_specified_javac=

  CLASSPATH_WITH_ECJ
  CLASSPATH_WITH_JAVAC

  if test "x${ECJ}" = x && test "x${JAVAC}" = x && test "x${user_specified_javac}" != xecj; then
      AC_MSG_ERROR([cannot find javac, try --with-ecj])
  fi
])

AC_DEFUN([CLASSPATH_WITH_ECJ],
[
  AC_ARG_WITH([ecj],
	      [AS_HELP_STRING(--with-ecj,bytecode compilation with ecj)],
  [
    if test "x${withval}" != x && test "x${withval}" != xyes && test "x${withval}" != xno; then
      CLASSPATH_CHECK_ECJ(${withval})
    else
      if test "x${withval}" != xno; then
        CLASSPATH_CHECK_ECJ
      fi
    fi
    user_specified_javac=ecj
  ],
  [ 
    CLASSPATH_CHECK_ECJ
  ])
  JAVAC="${ECJ} -nowarn"
  AC_SUBST(JAVAC)
])

AC_DEFUN([CLASSPATH_CHECK_ECJ],
[
  if test "x$1" != x; then
    if test -f "$1"; then
      ECJ="$1"
    else
      AC_PATH_PROG(ECJ, "$1")
    fi
  else
    AC_PATH_PROG(ECJ, "ecj")
    if test -z "${ECJ}"; then
      AC_PATH_PROG(ECJ, "ecj-3.1")
    fi
    if test -z "${ECJ}"; then
      AC_PATH_PROG(ECJ, "ecj-3.2")
    fi
    if test -z "${ECJ}"; then
      AC_PATH_PROG(ECJ, "ecj-3.3")
    fi
  fi
])

AC_DEFUN([CLASSPATH_WITH_JAVAC],
[
  AC_ARG_WITH([javac],
	      [AS_HELP_STRING(--with-javac,bytecode compilation with javac)],
  [
    if test "x${withval}" != x && test "x${withval}" != xyes && test "x${withval}" != xno; then
      CLASSPATH_CHECK_JAVAC(${withval})
    else
      if test "x${withval}" != xno; then
        CLASSPATH_CHECK_JAVAC
      fi
    fi
    user_specified_javac=javac
  ],
  [ 
    CLASSPATH_CHECK_JAVAC
  ])
  AC_SUBST(JAVAC)
])

AC_DEFUN([CLASSPATH_CHECK_JAVAC],
[
  if test "x$1" != x; then
    if test -f "$1"; then
      JAVAC="$1"
    else
      AC_PATH_PROG(JAVAC, "$1")
    fi
  else
    AC_PATH_PROG(JAVAC, "javac")
  fi
])

AC_DEFUN([FIND_JAVA],
[
  AC_ARG_WITH([java],
              [AS_HELP_STRING(--with-java,specify location of the 1.5 java vm)],
  [
    if test -f "${withval}"; then
      AC_MSG_CHECKING(java)
      JAVA="${withval}"
      AC_MSG_RESULT(${withval})
    else
      AC_PATH_PROG(JAVA, "${withval}")
    fi
  ],
  [
    JAVA=
  ])
  if test -z "${JAVA}"; then
    AC_PATH_PROG(JAVA, "gij")
  fi
  if test -z "${JAVA}"; then
    AC_PATH_PROG(JAVA, "java")
  fi
  if test -z "${JAVA}"; then
    AC_MSG_ERROR("A 1.5-compatible Java VM is required.")
  fi
  AC_SUBST(JAVA)
])

AC_DEFUN([WITH_OPENJDK_SRC_DIR],
[
  AC_MSG_CHECKING(for an OpenJDK source directory)
  AC_ARG_WITH([openjdk-src-dir],
              [AS_HELP_STRING(--with-openjdk-src-dir,specify the location of the openjdk sources)],
  [
    OPENJDK_SRC_DIR=${withval}
    AC_MSG_RESULT(${withval})
    conditional_with_openjdk_sources=true
  ],
  [ 
    conditional_with_openjdk_sources=false
    OPENJDK_SRC_DIR=`pwd`/openjdk
    AC_MSG_RESULT(${OPENJDK_SRC_DIR})
  ])
  AC_SUBST(OPENJDK_SRC_DIR)
  AM_CONDITIONAL(OPENJDK_SRC_DIR_FOUND, test "x${conditional_with_openjdk_sources}" = xtrue)
])

AC_DEFUN([FIND_ECJ_JAR],
[
  AC_ARG_WITH([ecj-jar],
              [AS_HELP_STRING(--with-ecj-jar,specify location of the ECJ jar)],
  [
    if test -f "${withval}"; then
      AC_MSG_CHECKING(for an ecj jar)
      ECJ_JAR="${withval}"
      AC_MSG_RESULT(${withval})
    fi
  ],
  [
    ECJ_JAR=
  ])
  if test -z "${ECJ_JAR}"; then
    AC_MSG_CHECKING(for eclipse-ecj.jar)
    if test -e "/usr/share/java/eclipse-ecj.jar"; then
      ECJ_JAR=/usr/share/java/eclipse-ecj.jar
      AC_MSG_RESULT(${ECJ_JAR})
    elif test -e "/usr/share/java/ecj.jar"; then
      ECJ_JAR=/usr/share/java/ecj.jar
      AC_MSG_RESULT(${ECJ_JAR})
    elif test -e "/usr/share/eclipse-ecj-3.3/lib/ecj.jar"; then
      ECJ_JAR=/usr/share/eclipse-ecj-3.3/lib/ecj.jar
      AC_MSG_RESULT(${ECJ_JAR})
    elif test -e "/usr/share/eclipse-ecj-3.2/lib/ecj.jar"; then
      ECJ_JAR=/usr/share/eclipse-ecj-3.2/lib/ecj.jar
      AC_MSG_RESULT(${ECJ_JAR})
    elif test -e "/usr/share/eclipse-ecj-3.1/lib/ecj.jar"; then
      ECJ_JAR=/usr/share/eclipse-ecj-3.1/lib/ecj.jar
      AC_MSG_RESULT(${ECJ_JAR})
    else
      AC_MSG_RESULT(no)
    fi
  fi
  if test -z "${ECJ_JAR}"; then
    AC_MSG_ERROR("A ECJ jar was not found.")
  fi
  AC_SUBST(ECJ_JAR)
])

AC_DEFUN([FIND_LIBGCJ_JAR],
[
  AM_CONDITIONAL(GCC_OLD, test x != x)
  AC_ARG_WITH([libgcj-jar],
              [AS_HELP_STRING(--with-libgcj-jar,specify location of the libgcj 4.3.x jar)],
  [
    if test -f "${withval}"; then
      AC_MSG_CHECKING(for libgcj jar)
      LIBGCJ_JAR="${withval}"
      AC_MSG_RESULT(${withval})
    fi
  ],
  [
    LIBGCJ_JAR=
  ])
  if test -z "${LIBGCJ_JAR}"; then
    AC_MSG_CHECKING(for libgcj-4.3.*.jar, libgcj-4.2.*.jar or libgcj-4.1.*.jar)
    for jar in /usr/share/java/libgcj-4.3*.jar; do
      test -e $jar && LIBGCJ_JAR=$jar
    done
    if test -n "${LIBGCJ_JAR}"; then
      AC_MSG_RESULT(${LIBGCJ_JAR})
    else
      AM_CONDITIONAL(GCC_OLD, test x = x)
      for jar in /usr/share/java/libgcj-4.1*.jar /usr/share/java/libgcj-4.2*.jar; do
	test -e $jar && LIBGCJ_JAR=$jar
      done
      if test -n ${LIBGCJ_JAR}; then
	AC_MSG_RESULT(${LIBGCJ_JAR})
      else
 	AC_MSG_RESULT(no)
      fi
    fi
  fi
  if test -z "${LIBGCJ_JAR}"; then
    AC_MSG_ERROR("A LIBGCJ jar was not found.")
  fi
  AC_SUBST(LIBGCJ_JAR)
])

AC_DEFUN([FIND_JAVAH],
[
  AC_ARG_WITH([javah],
              [AS_HELP_STRING(--with-javah,specify location of the javah)],
  [
    if test -f "${withval}"; then
      AC_MSG_CHECKING(for javah)
      JAVAH="${withval}"
      AC_MSG_RESULT(${withval})
    else
      AC_PATH_PROG(JAVAH, "${withval}")
    fi
  ],
  [
    JAVAH=
  ])
  if test -z "${JAVAH}"; then
    AC_PATH_PROG(JAVAH, "gjavah")
  fi
  if test -z "${JAVAH}"; then
    AC_PATH_PROG(JAVAH, "javah")
  fi
  if test -z "${JAVAH}"; then
    AC_MSG_ERROR("javah was not found.")
  fi
  AC_SUBST(JAVAH)
])

AC_DEFUN([FIND_JAR],
[
  AC_ARG_WITH([jar],
              [AS_HELP_STRING(--with-jar,specify location of the jar)],
  [
    if test -f "${withval}"; then
      AC_MSG_CHECKING(for jar)
      JAR="${withval}"
      AC_MSG_RESULT(${withval})
    else
      AC_PATH_PROG(JAR, "${withval}")
    fi
  ],
  [
    JAR=
  ])
  if test -z "${JAR}"; then
    AC_PATH_PROG(JAR, "gjar")
  fi
  if test -z "${JAR}"; then
    AC_PATH_PROG(JAR, "jar")
  fi
  if test -z "${JAR}"; then
    AC_MSG_ERROR("jar was not found.")
  fi
  AC_MSG_CHECKING([whether jar supports @<file> argument])
  touch _config.txt
  cat >_config.list <<EOF
_config.txt
EOF
  if $JAR cf _config.jar @_config.list 2>/dev/null; then
    JAR_KNOWS_ATFILE=1
    AC_MSG_RESULT(yes)
  else
    JAR_KNOWS_ATFILE=
    AC_MSG_RESULT(no)
  fi
  AC_MSG_CHECKING([whether jar supports stdin file arguments])
  if cat _config.list | $JAR cf@ _config.jar 2>/dev/null; then
    JAR_ACCEPTS_STDIN_LIST=1
    AC_MSG_RESULT(yes)
  else
    JAR_ACCEPTS_STDIN_LIST=
    AC_MSG_RESULT(no)
  fi
  rm -f _config.list _config.jar
  AC_MSG_CHECKING([whether jar supports -J options at the end])
  if $JAR cf _config.jar _config.txt -J-Xmx896m 2>/dev/null; then
    JAR_KNOWS_J_OPTIONS=1
    AC_MSG_RESULT(yes)
  else
    JAR_KNOWS_J_OPTIONS=
    AC_MSG_RESULT(no)
  fi
  rm -f _config.txt _config.jar
  AC_SUBST(JAR)
  AC_SUBST(JAR_KNOWS_ATFILE)
  AC_SUBST(JAR_ACCEPTS_STDIN_LIST)
  AC_SUBST(JAR_KNOWS_J_OPTIONS)
])

AC_DEFUN([FIND_RMIC],
[
  AC_ARG_WITH([rmic],
              [AS_HELP_STRING(--with-rmic,specify location of the rmic)],
  [
    if test -f "${withval}"; then
      AC_MSG_CHECKING(for rmic)
      RMIC="${withval}"
      AC_MSG_RESULT(${withval})
    else
      AC_PATH_PROG(RMIC, "${withval}")
    fi
  ],
  [
    RMIC=
  ])
  if test -z "${RMIC}"; then
    AC_PATH_PROG(RMIC, "grmic")
  fi
  if test -z "${RMIC}"; then
    AC_PATH_PROG(RMIC, "rmic")
  fi
  if test -z "${RMIC}"; then
    AC_MSG_ERROR("rmic was not found.")
  fi
  AC_SUBST(RMIC)
])

AC_DEFUN([FIND_ENDORSED_JARS],
[
  AC_MSG_CHECKING(for endorsed jars dir)
  AC_ARG_WITH([endorsed-dir],
              [AS_HELP_STRING(--with-endorsed-dir,specify directory of endorsed jars (xalan-j2.jar, xalan-j2-serializer.jar, xerces-j2.jar))],
  [
    if test "x${withval}" = "xno"; then
        ENDORSED_JARS="${withval}"
        AC_MSG_RESULT(${withval})
    else if test -f "${withval}/xalan-j2.jar"; then
      if test -f "${withval}/xalan-j2-serializer.jar"; then
        if test -f "${withval}/xerces-j2.jar"; then
          ENDORSED_JARS="${withval}"
          AC_MSG_RESULT(${withval})
        fi
      fi
    fi
  ],
  [
    ENDORSED_JARS=
  ])
  if test -z "${ENDORSED_JARS}"; then
    if test -f "/usr/share/java/xalan-j2.jar"; then
      if test -f "/usr/share/java/xalan-j2-serializer.jar"; then
        if test -f "/usr/share/java/xerces-j2.jar"; then
          ENDORSED_JARS="/usr/share/java/xalan-j2.jar /usr/share/java/xalan-j2-serializer.jar /usr/share/java/xerces-j2.jar"
          AC_MSG_RESULT(/usr/share/java)
        fi
      fi
    fi
    if test -z "${ENDORSED_JARS}"; then
      AC_MSG_RESULT(missing)
    fi
  fi
  if test -z "${ENDORSED_JARS}"; then
    AC_MSG_ERROR("A directory containing required jars (xalan-j2.jar, xalan-j2-serializer.jar, xerces-j2.jar) was not found.")
  fi
  AC_SUBST(ENDORSED_JARS)
])

AC_DEFUN([WITH_OPENJDK_SRC_ZIP],
[
  AC_MSG_CHECKING(for an OpenJDK source zip)
  AC_ARG_WITH([openjdk-src-zip],
              [AS_HELP_STRING(--with-openjdk-src-zip,specify the location of the openjdk source zip)],
  [
    ALT_OPENJDK_SRC_ZIP=${withval}
    AM_CONDITIONAL(USE_ALT_OPENJDK_SRC_ZIP, test x = x)
  ],
  [ 
    ALT_OPENJDK_SRC_ZIP="not specified"
    AM_CONDITIONAL(USE_ALT_OPENJDK_SRC_ZIP, test x != x)
  ])
  AC_MSG_RESULT(${ALT_OPENJDK_SRC_ZIP})
  AC_SUBST(ALT_OPENJDK_SRC_ZIP)
])

AC_DEFUN([WITH_VISUALVM_SRC_ZIP],
[
  AC_MSG_CHECKING(for a VisualVM source zip)
  AC_ARG_WITH([visualvm-src-zip],
              [AS_HELP_STRING(--with-visualvm-src-zip, specify the location of the visualvm source zip)],
  [
    ALT_VISUALVM_SRC_ZIP=${withval}
    AM_CONDITIONAL(USE_ALT_VISUALVM_SRC_ZIP, test x = x)
  ],
  [ 
    ALT_VISUALVM_SRC_ZIP="not specified"
    AM_CONDITIONAL(USE_ALT_VISUALVM_SRC_ZIP, test x != x)
  ])
  AC_MSG_RESULT(${ALT_VISUALVM_SRC_ZIP})
  AC_SUBST(ALT_VISUALVM_SRC_ZIP)
])

AC_DEFUN([WITH_NETBEANS_PROFILER_SRC_ZIP],
[
  AC_MSG_CHECKING(for a NetBeans profiler source zip)
  AC_ARG_WITH([netbeans-profiler-src-zip],
              [AS_HELP_STRING(--with-netbeans-profiler-src-zip, specify the location of the netbeans profiler source zip)],
  [
    ALT_NETBEANS_PROFILER_SRC_ZIP=${withval}
    AM_CONDITIONAL(USE_ALT_NETBEANS_PROFILER_SRC_ZIP, test x = x)
  ],
  [ 
    ALT_NETBEANS_PROFILER_SRC_ZIP="not specified"
    AM_CONDITIONAL(USE_ALT_NETBEANS_PROFILER_SRC_ZIP, test x != x)
  ])
  AC_MSG_RESULT(${ALT_NETBEANS_PROFILER_SRC_ZIP})
  AC_SUBST(ALT_NETBEANS_PROFILER_SRC_ZIP)
])

AC_DEFUN([WITH_NETBEANS_BASIC_CLUSTER_SRC_ZIP],
[
  AC_MSG_CHECKING(for a NetBeans basic cluster source zip)
  AC_ARG_WITH([netbeans-basic-cluster-src-zip],
              [AS_HELP_STRING(--with-netbeans-basic-cluster-src-zip, specify the location of the netbeans basic cluster source zip)],
  [
    ALT_NETBEANS_BASIC_CLUSTER_SRC_ZIP=${withval}
    AM_CONDITIONAL(USE_ALT_NETBEANS_BASIC_CLUSTER_SRC_ZIP, test x = x)
  ],
  [ 
    ALT_NETBEANS_BASIC_CLUSTER_SRC_ZIP="not specified"
    AM_CONDITIONAL(USE_ALT_NETBEANS_BASIC_CLUSTER_SRC_ZIP, test x != x)
  ])
  AC_MSG_RESULT(${ALT_NETBEANS_BASIC_CLUSTER_SRC_ZIP})
  AC_SUBST(ALT_NETBEANS_BASIC_CLUSTER_SRC_ZIP)
])

AC_DEFUN([WITH_ALT_JAR_BINARY],
[
  AC_MSG_CHECKING(for an alternate jar command)
  AC_ARG_WITH([alt-jar],
              [AS_HELP_STRING(--with-alt-jar, specify the location of an alternate jar binary to use for building)],
  [
    ALT_JAR_CMD=${withval}
    AM_CONDITIONAL(USE_ALT_JAR, test x = x)
  ],
  [ 
    ALT_JAR_CMD="not specified"
    AM_CONDITIONAL(USE_ALT_JAR, test x != x)
  ])
  AC_MSG_RESULT(${ALT_JAR_CMD})
  AC_SUBST(ALT_JAR_CMD)
])

AC_DEFUN([FIND_XALAN2_JAR],
[
  AC_MSG_CHECKING(xalan2 jar)
  AC_ARG_WITH([xalan2-jar],
              [AS_HELP_STRING(--with-xalan2-jar,specify location of the xalan2 jar)],
  [
    if test -f "${withval}" ; then
      XALAN2_JAR="${withval}"
    fi
  ],
  [
    XALAN2_JAR=
  ])
  if test -z "${XALAN2_JAR}"; then
    if test -e "/usr/share/java/xalan-j2.jar"; then
      XALAN2_JAR=/usr/share/java/xalan-j2.jar
    elif test -e "/usr/share/java/xalan2.jar"; then
      XALAN2_JAR=/usr/share/java/xalan2.jar
    elif test -e "/usr/share/xalan/lib/xalan.jar"; then
      XALAN2_JAR=/usr/share/xalan/lib/xalan.jar
    else
      AC_MSG_RESULT(no)
    fi
  fi
  if test -z "${XALAN2_JAR}"; then
    AC_MSG_ERROR("A xalan2 jar was not found.")
  fi
  AC_MSG_RESULT(${XALAN2_JAR})
  AC_SUBST(XALAN2_JAR)
])

AC_DEFUN([FIND_XALAN2_SERIALIZER_JAR],
[
  AC_MSG_CHECKING(for xalan2 serializer jar)
  AC_ARG_WITH([xalan2-serializer-jar],
              [AS_HELP_STRING(--with-xalan2-serializer-jar,specify location of the xalan2-serializer jar)],
  [
    if test -f "${withval}" ; then
      XALAN2_SERIALIZER_JAR="${withval}"
    fi
  ],
  [
    XALAN2_SERIALIZER_JAR=
  ])
  if test -z "${XALAN2_SERIALIZER_JAR}"; then
    if test -e "/usr/share/java/xalan-j2-serializer.jar"; then
      XALAN2_SERIALIZER_JAR=/usr/share/java/xalan-j2-serializer.jar
    elif test -e "/usr/share/xalan-serializer/lib/serializer.jar"; then
      XALAN2_SERIALIZER_JAR=/usr/share/xalan-serializer/lib/serializer.jar
    elif test -e "/usr/share/java/serializer.jar"; then
      XALAN2_SERIALIZER_JAR=/usr/share/java/serializer.jar
    else
      AC_MSG_RESULT(no)
    fi
  fi
  if test -z "${XALAN2_SERIALIZER_JAR}"; then
    AC_MSG_ERROR("A xalan2-serializer jar was not found.")
  fi
  AC_MSG_RESULT(${XALAN2_SERIALIZER_JAR})
  AC_SUBST(XALAN2_SERIALIZER_JAR)
])

AC_DEFUN([FIND_XERCES2_JAR],
[
  AC_MSG_CHECKING(for xerces2 jar)
  AC_ARG_WITH([xerces2-jar],
              [AS_HELP_STRING(--with-xerces2-jar,specify location of the xerces2 jar)],
  [
    if test -f "${withval}" ; then
      XERCES2_JAR="${withval}"
    fi
  ],
  [
    XERCES2_JAR=
  ])
  if test -z "${XERCES2_JAR}"; then
    if test -e "/usr/share/java/xerces-j2.jar"; then
      XERCES2_JAR=/usr/share/java/xerces-j2.jar
    elif test -e "/usr/share/java/xerces2.jar"; then
      XERCES2_JAR=/usr/share/java/xerces2.jar
    elif test -e "/usr/share/xerces-2/lib/xercesImpl.jar"; then
      XERCES2_JAR=/usr/share/xerces-2/lib/xercesImpl.jar
    elif test -e "/usr/share/java/xercesImpl.jar"; then
      XERCES2_JAR=/usr/share/java/xercesImpl.jar
    else
      AC_MSG_RESULT(no)
    fi
  fi
  if test -z "${XERCES2_JAR}"; then
    AC_MSG_ERROR("A xerces2 jar was not found.")
  fi
  AC_MSG_RESULT(${XERCES2_JAR})
  AC_SUBST(XERCES2_JAR)
])

AC_DEFUN([FIND_NETBEANS],
[
  AC_ARG_WITH([netbeans],
              [AS_HELP_STRING(--with-netbeans,specify location of netbeans)],
  [
    if test -f "${withval}"; then
      AC_MSG_CHECKING(netbeans)
      NETBEANS="${withval}"
      AC_MSG_RESULT(${withval})
    else
      AC_PATH_PROG(NETBEANS, "${withval}")
    fi
  ],
  [
    NETBEANS=
  ])
  if test -z "${NETBEANS}"; then
    AC_PATH_PROG(NETBEANS, "netbeans")
  fi
  if test -z "${NETBEANS}"; then
    AC_MSG_ERROR("NetBeans was not found.")
  fi
  AC_SUBST(NETBEANS)
])

AC_DEFUN([FIND_RHINO_JAR],
[
  AC_MSG_CHECKING(whether to include Javascript support via Rhino)
  AC_ARG_WITH([rhino],
              [AS_HELP_STRING(--with-rhino,specify location of the rhino jar)],
  [
    case "${withval}" in
      yes)
	RHINO_JAR=yes
        ;;
      no)
        RHINO_JAR=no
        ;;
      *)
    	if test -f "${withval}"; then
          RHINO_JAR="${withval}"
        else
	  AC_MSG_RESULT([not found])
          AC_MSG_ERROR("The rhino jar ${withval} was not found.")
        fi
	;;
     esac
  ],
  [
    RHINO_JAR=yes
  ])
  if test x"${RHINO_JAR}" = "xyes"; then
    if test -e "/usr/share/java/rhino.jar"; then
      RHINO_JAR=/usr/share/java/rhino.jar
    elif test -e "/usr/share/java/js.jar"; then
      RHINO_JAR=/usr/share/java/js.jar
    fi
    if test x"${RHINO_JAR}" = "xyes"; then
      AC_MSG_RESULT([not found])
      AC_MSG_ERROR("A rhino jar was not found in /usr/share/java as either rhino.jar or js.jar.")
    fi
  fi
  AC_MSG_RESULT(${RHINO_JAR})
  AM_CONDITIONAL(WITH_RHINO, test x"${RHINO_JAR}" != "xno")
  AC_SUBST(RHINO_JAR)
])

AC_DEFUN([FIND_PULSEAUDIO],
[
  AC_PATH_PROG(PULSEAUDIO_BIN, "pulseaudio")
  if test -z "${PULSEAUDIO_BIN}"; then
    AC_MSG_ERROR("pulseaudio was not found.")
  fi
  AC_SUBST(PULSEAUDIO_BIN)
])

AC_DEFUN([ENABLE_OPTIMIZATIONS],
[
  AC_MSG_CHECKING(whether to disable optimizations)
  AC_ARG_ENABLE([optimizations],
                [AS_HELP_STRING(--disable-optimizations,build with -O0 -g [[default=no]])],
  [
    case "${enableval}" in
      no)
        AC_MSG_RESULT([yes, building with -O0 -g])
        enable_optimizations=no
        ;;
      *)
        AC_MSG_RESULT([no])
        enable_optimizations=yes
        ;;
    esac
  ],
  [
    enable_optimizations=yes
  ])
  AM_CONDITIONAL([ENABLE_OPTIMIZATIONS], test x"${enable_optimizations}" = "xyes")
])

AC_DEFUN([FIND_TOOL],
[AC_PATH_TOOL([$1],[$2])
 if test x"$$1" = x ; then
   AC_MSG_ERROR([$2 program not found in PATH])
 fi
 AC_SUBST([$1])
])

AC_DEFUN([ENABLE_ZERO_BUILD],
[
  AC_MSG_CHECKING(whether to use the zero-assembler port)
  use_zero=no
  AC_ARG_ENABLE([zero],
                [AS_HELP_STRING(--enable-zero,
                               use zero-assembler port on non-zero platforms)],
  [
    case "${enableval}" in
      no)
        use_zero=no
        ;;
      *)
        use_zero=yes
        ;;
    esac
  ],
  [
    case "${host}" in
      i?86-*-*) ;;
      sparc*-*-*) ;;
      x86_64-*-*) ;;
      *)
        if test "x${WITH_CACAO}" != xno; then
          use_zero=no
        else
          use_zero=yes
        fi
        ;;
    esac
  ])
  AC_MSG_RESULT($use_zero)
  AM_CONDITIONAL(ZERO_BUILD, test "x${use_zero}" = xyes)

  ZERO_LIBARCH=
  ZERO_BITSPERWORD=
  ZERO_ENDIANNESS=
  ZERO_ARCHDEF=
  ZERO_ARCHFLAG=
  if test "x${use_zero}" = xyes; then
    ZERO_LIBARCH="${INSTALL_ARCH_DIR}"
    dnl can't use AC_CHECK_SIZEOF on multilib
    case "${ZERO_LIBARCH}" in
      i386|ppc|s390|sparc)
        ZERO_BITSPERWORD=32
        ;;
      amd64|ppc64|s390x|sparc64)
        ZERO_BITSPERWORD=64
        ;;
      *)
        AC_CHECK_SIZEOF(void *)
        ZERO_BITSPERWORD=`expr "${ac_cv_sizeof_void_p}" "*" 8`
    esac
    AC_C_BIGENDIAN([ZERO_ENDIANNESS="big"], [ZERO_ENDIANNESS="little"])
    case "${ZERO_LIBARCH}" in
      i386)
        ZERO_ARCHDEF="IA32"
        ;;
      ppc*)
        ZERO_ARCHDEF="PPC"
        ;;
      s390*)
        ZERO_ARCHDEF="S390"
        ;;
      sparc*)
        ZERO_ARCHDEF="SPARC"
        ;;
      *)
        ZERO_ARCHDEF=`echo ${ZERO_LIBARCH} | tr a-z A-Z`
    esac
    dnl multilib machines need telling which mode to build for
    case "${ZERO_LIBARCH}" in
      i386|ppc|sparc)
        ZERO_ARCHFLAG="-m32"
        ;;
      s390)
        ZERO_ARCHFLAG="-m31"
        ;;
      amd64|ppc64|s390x|sparc64)
        ZERO_ARCHFLAG="-m64"
        ;;
    esac
  fi
  AC_SUBST(ZERO_LIBARCH)
  AC_SUBST(ZERO_BITSPERWORD)
  AC_SUBST(ZERO_ENDIANNESS)
  AC_SUBST(ZERO_ARCHDEF)
  AC_SUBST(ZERO_ARCHFLAG)
  AC_CONFIG_FILES([platform_zero])
  AC_CONFIG_FILES([jvm.cfg])
  AC_CONFIG_FILES([ergo.c])
])

AC_DEFUN([SET_CORE_OR_SHARK_BUILD],
[
  AC_MSG_CHECKING(whether to use the Shark JIT)
  shark_selected=no
  AC_ARG_ENABLE([shark], [AS_HELP_STRING(--enable-shark, use Shark JIT)],
  [
    case "${enableval}" in
      no)
        ;;
      *)
        shark_selected=yes
        ;;
    esac
  ])

  use_core=no
  use_shark=no
  if test "x${WITH_CACAO}" != "xno"; then
    use_core=yes
  elif test "x${use_zero}" = "xyes"; then
    if test "x${shark_selected}" = "xyes"; then
      use_shark=yes
    else
      use_core=yes
    fi
  fi
  AC_MSG_RESULT($use_shark)

  AM_CONDITIONAL(CORE_BUILD, test "x${use_core}" = xyes)
  AM_CONDITIONAL(SHARK_BUILD, test "x${use_shark}" = xyes)
])

AC_DEFUN([AC_CHECK_ENABLE_CACAO],
[
  AC_MSG_CHECKING(whether to use CACAO as VM)
  AC_ARG_ENABLE([cacao],
	      [AS_HELP_STRING(--enable-cacao,use CACAO as VM [[default=no]])],
  [
    WITH_CACAO="${enableval}"
  ],
  [
    WITH_CACAO=no
  ])

  AC_MSG_RESULT(${WITH_CACAO})
  AM_CONDITIONAL(WITH_CACAO, test x"${WITH_CACAO}" = "xyes")
  AC_SUBST(WITH_CACAO)
])

AC_DEFUN([AC_CHECK_WITH_CACAO_HOME],
[
  AC_MSG_CHECKING(for CACAO home directory)
  AC_ARG_WITH([cacao-home],
              [AS_HELP_STRING([--with-cacao-home],
                              [CACAO home directory [[default=/usr/local/cacao]]])],
              [
                case "${withval}" in
                yes)
                  CACAO_IMPORT_PATH=/usr/local/cacao
                  ;;
                *)
                  CACAO_IMPORT_PATH=${withval}
                  ;;
                esac
                AM_CONDITIONAL(USE_SYSTEM_CACAO, true)
              ],
              [
                CACAO_IMPORT_PATH="\$(abs_top_builddir)/cacao/install"
                AM_CONDITIONAL(USE_SYSTEM_CACAO, false)
              ])
  AC_MSG_RESULT(${CACAO_IMPORT_PATH})
  AC_SUBST(CACAO_IMPORT_PATH)
])

AC_DEFUN([AC_CHECK_WITH_CACAO_SRC_ZIP],
[
  AC_MSG_CHECKING(for a CACAO source zip)
  AC_ARG_WITH([cacao-src-zip],
              [AS_HELP_STRING(--with-cacao-src-zip,specify the location of the CACAO source zip)],
  [
    ALT_CACAO_SRC_ZIP=${withval}
    AM_CONDITIONAL(USE_ALT_CACAO_SRC_ZIP, test x = x)
  ],
  [ 
    ALT_CACAO_SRC_ZIP="not specified"
    AM_CONDITIONAL(USE_ALT_CACAO_SRC_ZIP, test x != x)
  ])
  AC_MSG_RESULT(${ALT_CACAO_SRC_ZIP})
  AC_SUBST(ALT_CACAO_SRC_ZIP)
])

AC_DEFUN([AC_CHECK_WITH_GCJ],
[
  AC_MSG_CHECKING([whether to compile ecj natively])
  AC_ARG_WITH([gcj],
	      [AS_HELP_STRING(--with-gcj,location of gcj for natively compiling ecj)],
  [
    GCJ="${withval}"
  ],
  [ 
    GCJ="no"
  ])
  AC_MSG_RESULT([${GCJ}])
  if test "x${GCJ}" = xyes; then
    AC_PATH_TOOL([GCJ],[gcj])
  fi
  AC_SUBST([GCJ])
])

AC_DEFUN([AC_CHECK_WITH_HOTSPOT_BUILD],
[
  DEFAULT_BUILD="14.0b08"
  AC_MSG_CHECKING([which HotSpot build to use])
  AC_ARG_WITH([hotspot-build],
	      [AS_HELP_STRING(--with-hotspot-build,the HotSpot build to use)],
  [
    HSBUILD="${withval}"
  ],
  [ 
    HSBUILD="${DEFAULT_BUILD}"
  ])
  if test "x${HSBUILD}" = xyes; then
	HSBUILD="${DEFAULT_BUILD}"
  elif test "x${HSBUILD}" = xno; then
	HSBUILD="original"
  fi
  AC_MSG_RESULT([${HSBUILD}])
  AC_SUBST([HSBUILD])
  AM_CONDITIONAL(WITH_ALT_HSBUILD, test "x${HSBUILD}" != "xoriginal")
])

AC_DEFUN([WITH_HOTSPOT_SRC_ZIP],
[
  AC_MSG_CHECKING(for a HotSpot source zip)
  AC_ARG_WITH([hotspot-src-zip],
              [AS_HELP_STRING(--with-hotspot-src-zip,specify the location of the hotspot source zip)],
  [
    ALT_HOTSPOT_SRC_ZIP=${withval}
    AM_CONDITIONAL(USE_ALT_HOTSPOT_SRC_ZIP, test x = x)
  ],
  [ 
    ALT_HOTSPOT_SRC_ZIP="not specified"
    AM_CONDITIONAL(USE_ALT_HOTSPOT_SRC_ZIP, test x != x)
  ])
  AC_MSG_RESULT(${ALT_HOTSPOT_SRC_ZIP})
  AC_SUBST(ALT_HOTSPOT_SRC_ZIP)
])

AC_DEFUN([ENABLE_HG],
[
  AC_MSG_CHECKING(whether to retrieve the source code from Mercurial)
  AC_ARG_ENABLE([hg],
                [AS_HELP_STRING(--enable-hg,download source code from Mercurial [[default=no]])],
  [
    case "${enableval}" in
      no)
	enable_hg=no
        ;;
      *)
        enable_hg=yes
        ;;
    esac
  ],
  [
        enable_hg=no
  ])
  AC_MSG_RESULT([${enable_hg}])
  AM_CONDITIONAL([USE_HG], test x"${enable_hg}" = "xyes")
])

AC_DEFUN([AC_CHECK_WITH_HG_REVISION],
[
  AC_MSG_CHECKING([which Mercurial revision to use])
  AC_ARG_WITH([hg-revision],
	      [AS_HELP_STRING(--with-hg-revision,the Mercurial revision to use)],
  [
    HGREV="${withval}"
    AC_MSG_RESULT([${HGREV}])
  ],
  [ 
    HGREV=""
    AC_MSG_RESULT([tip])
  ])
  AC_SUBST([HGREV])
  AM_CONDITIONAL(WITH_HGREV, test "x${HGREV}" != "x")
])
