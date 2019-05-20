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
%{!?source: %define source %{name}_%{version}-%{release}.tar.gz}

Name:          push-notification-kafka-plugin
Summary:       Dovecot push notification driver for Kafka
Version:       0.0.5
Release:       0%{?dist}
URL:           https://github.com/tallence/push-notification-kafka-plugin
Group:         Productivity/Networking/Email/Servers
License:       LGPL-2.1
Source:        %{source}

Provides:      push-notification-kafka-plugin = %{version}-%{release}

BuildRoot:     %{_tmppath}/%{name}-%{version}-build
BuildRequires: %dovecot_devel
BuildRequires: librdkafka-devel >= %librdkafka_version
BuildRequires: gcc automake libtool
BuildRequires: pkg-config

%description
This package provides a Kafka driver for Dovecot's push notification framework.

%prep
%setup -q

%build
export CFLAGS="%{optflags}"
%if 0%{?suse_version} > 1000
    export CFLAGS="$CFLAGS -fstack-protector"
%endif
export CFLAGS="$CFLAGS -fpic -DPIC"
export LIBS="-pie"

./autogen.sh
%configure \
	--prefix=%{_prefix} \
	--with-dovecot=%{_libdir}/dovecot
%{__make}

%install
%makeinstall

# clean up unused files
find %{buildroot}%{_libdir}/ -type f -name \*.la -delete
find %{buildroot}%{_libdir}/dovecot/ -type f -name \*.a  -delete

%clean
%{__rm} -rf %{buildroot}

%post
/sbin/ldconfig
%postun
/sbin/ldconfig

%files
%defattr(-,root,root)
%dir %{_libdir}/dovecot
%{_libdir}/dovecot/lib*.so*

%changelog
