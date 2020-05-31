pushdef([AC_VERSION],esyscmd(awk '/^VERSION[[[:space:]]]+=[[[:space:]]]+/{printf $3}' Makefile.mk.in))
pushdef([AC_SUBVERSION],esyscmd(awk '/^SUBVERSION[[[:space:]]]+=[[[:space:]]]+/{printf $3}' Makefile.mk.in))
pushdef([AC_EXTRAVERSION],esyscmd(awk '/^EXTRAVERSION[[[:space:]]]+=[[[:space:]]]+/{printf $3}' Makefile.mk.in))
pushdef([MY_PACKAGE_VERSION],[AC_VERSION.AC_SUBVERSION.AC_EXTRAVERSION-esyscmd(date +%Y%m%d|tr -d '\n')])
AC_INIT([[huginn]],[MY_PACKAGE_VERSION],[[amok@codestation.org]])
dnl It is expected that client applications have link to yaal's _aux.
dnl This macro has to be here due to primitivism of libtoolize.
AC_CONFIG_AUX_DIR([_aux])
m4_include([_aux/client-configure.m4])
YAAL_CONFIGURE_CLIENT_INIT(huginn,MY_PACKAGE_VERSION)
if test ["x${REPL}"] = ["x"] -o ["x${REPL}"] = ["xreplxx"] ; then
	AC_CHECK_LIB([replxx${LIB_INFIX}],[replxx_input],,[REPL_FAIL=1])
	AC_CHECK_HEADERS([replxx.hxx],,[REPL_FAIL=1])
fi
if test ["x${REPL_FAIL}"] = ["x1"] -o ["x${REPL}"] = ["xeditline"] ; then
	AC_CHECK_LIB([edit],[el_gets],,[REPL_FAIL=2])
	AC_CHECK_HEADERS([histedit.h],,[REPL_FAIL=2])
else
	if test ["x${REPL_FAIL}"] = ["x"] -a \( ["x${REPL}"] = ["x"] -o ["x${REPL}"] = ["xreplxx"] \) ; then
		AC_DEFINE([USE_REPLXX],[],[Use replxx library.])
	fi
fi
if test ["x${REPL_FAIL}"] = ["x2"] -o ["x${REPL}"] = ["xreadline"] ; then
	AC_CHECK_LIB([readline],[readline],,AC_MSG_ERROR([Cannot continue without REPL library.]))
else
	if test ["x${REPL_FAIL}"] = ["x1"] -o ["x${REPL}"] = ["xeditline"] ; then
		AC_DEFINE([USE_EDITLINE],[],[Use editline library.])
	fi
fi
YAAL_CONFIGURE_CLIENT_FINALIZE
