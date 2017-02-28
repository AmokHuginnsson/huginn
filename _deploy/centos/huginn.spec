Name:    huginn
Version: %(awk '/^VERSION *=|SUBVERSION *=|EXTRAVERSION *=/{VERSION=VERSION DOT $3;DOT="."}END{print VERSION}' ./Makefile.mk.in)
Release: %{?BUILD_ID}%{!?BUILD_ID:1}%{?dist}
Summary: Huginn - programming language with no quirks, so simple every child can master it.

Group:   System Environment/Libraries
License: Commercialh
URL:     http://codestation.org/
Source:  https://codestation.org/repo/huginn.git
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires: yaal-devel, make, autoconf, libtool, libedit-devel
Requires: yaal

%global _enable_debug_package 0
%global debug_package %{nil}
%global __os_install_post /usr/lib/rpm/brp-compress %{nil}

%description
Huginn is a computer programming language with following traits:
* interpreted
* object oriented
* static type system
* strongly typed
* dynamically typed
* no quirks guarantee
* support arbitrary precision calculations per built-in type
* interperter/executor trivially embedable in C++ code

%define flagfilter sed -e 's/-O2\\>//g' -e 's/-g\\>//g' -e 's/-Wall\\>//g' -e 's/-Werror\\>//g' -e 's/-Wformat-security\\>//g' -e 's/-Wformat\\>//g'
%define clearflags export CFLAGS=`echo ${CFLAGS} | %{flagfilter}`;export CXXFLAGS=`echo ${CXXFLAGS} | %{flagfilter}`

%prep
umask 0077
if [ -f "${RPM_SOURCE_DIR}/%{name}-%{version}.tar.gz" ] ; then
%setup -T -c
else
	cd -
	/bin/rm -rf "${RPM_BUILD_DIR}/%{name}-%{version}"
	mkdir "${RPM_BUILD_DIR}/%{name}-%{version}"
	tar cf - --exclude build --exclude _deploy/centos/rpm . | tar -x -C "${RPM_BUILD_DIR}/%{name}-%{version}"
	cd -
fi
cd "%{name}-%{version}"
./setup.sh
make purge

%build
umask 0077
%{clearflags}
make %{?_smp_mflags} release doc PREFIX=%{_prefix} SYSCONFDIR=%{_sysconfdir} BINDIR=%{_bindir} DATADIR=%{_datadir}

%install
rm -rf ${RPM_BUILD_ROOT}
umask 0077
make install-release DESTDIR=${RPM_BUILD_ROOT}

%clean
rm -rf ${RPM_BUILD_ROOT}
umask 0077
make purge

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_sysconfdir}/*
%{_datadir}/huginn
%{_datadir}/doc/*
%{_datadir}/man/man1/*
%doc

%changelog

