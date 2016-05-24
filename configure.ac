pushdef([MY_PACKAGE_VERSION],[0.0.0-esyscmd(date +%Y%m%d|tr -d '\n')])
AC_INIT([[huginn]],[MY_PACKAGE_VERSION],[[amok@codestation.org]])
dnl It is expected that client applications have link to yaal's _aux.
dnl This macro has to be here due to primitivism of libtoolize.
AC_CONFIG_AUX_DIR([_aux])
m4_include([_aux/client-configure.m4])
YAAL_CONFIGURE_CLIENT_INIT(huginn,MY_PACKAGE_VERSION)
AC_CHECK_LIB([readline],[readline],,[AC_MSG_ERROR([Cannot continue without readline library.])])
YAAL_CONFIGURE_CLIENT_FINALIZE
