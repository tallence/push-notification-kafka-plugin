#
# spec file for package push-notification-kafka-plugin
#
# Copyright (c) 2017-2018 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

%{!?dovecot_devel: %define dovecot_devel dovecot22-devel}
%{!?librdkafka_version: %define librdkafka_version 0.11.4}

##############################################################################
# preamble
##############################################################################
Summary:       Dovecot push notification driver for Kafka
Name:          push-notification-kafka-plugin
Version:       0.0.1
Release:	   0%{?dist}
Group:	       Productivity/Networking/Email/Servers
License:	   LGPL-2.1
Source:        %{name}_%{version}-%{release}.tar.gz

%define dovecot_home        /opt/app/dovecot

BuildRequires: %dovecot_devel
BuildRequires: librdkafka-devel >= %librdkafka_version
BuildRequires: gcc automake libtool
BuildRequires: pkg-config
BuildRoot:     %{_tmppath}/%{name}-%{version}-build

%description
This package provides Kafka notifications for Dovecot push notification. 


##############################################################################
# prep section
##############################################################################
%prep
%setup -q


##############################################################################
# build section
##############################################################################
%build
export CFLAGS="%{optflags}"
%if 0%{?suse_version} > 1000
    export CFLAGS="$CFLAGS -fstack-protector"
%endif
export CFLAGS="$CFLAGS -fpic -DPIC"
export LIBS="-pie"
export ACLOCAL_DIR="%{dovecot_home}/share/aclocal"

./autogen.sh
%configure \
	--prefix=%{_prefix} \
	--with-dovecot=%{_libdir}/dovecot
%{__make}


##############################################################################
# install section
##############################################################################
%install
%makeinstall

# clean up unused files
find %{buildroot}%{dovecot_home}/lib/dovecot/ -type f -name \*.la -delete
find %{buildroot}%{dovecot_home}/lib/dovecot/ -type f -name \*.a  -delete


##############################################################################
# clean section
##############################################################################
%clean
%{__rm} -rf %{buildroot}


##############################################################################
# post section
##############################################################################
%post
/sbin/ldconfig
%postun
/sbin/ldconfig


##############################################################################
# file list
##############################################################################
%files
%defattr(-,root,root)
%dir %{_libdir}/dovecot
%{_libdir}/dovecot/lib*.so*


##############################################################################
# changelog
##############################################################################
%changelog
