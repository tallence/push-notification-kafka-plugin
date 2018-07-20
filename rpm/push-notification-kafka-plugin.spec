# norootforbuild

##############################################################################
# preamble
##############################################################################
Summary:       Dovecot push notification driver for Kafka
Name:          push-notification-kafka-plugin
Version:       0.0.1
Release:       1
Group:	       Productivity/Networking/Email/Servers
License:	   LGPL-2.1
Source:        %{name}_%{version}-%{release}.tar.gz

%define dovecot_home        /opt/app/dovecot

Requires:      dovecot >= 2.2.21-2.1

BuildRequires: gcc automake libtool
BuildRequires: dovecot-devel >= 2.2.21-2.1

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
./configure --prefix=%{dovecot_home}
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
# file list
##############################################################################
%files
%defattr(-,root,root)

%dir /opt/app/
%dir %{dovecot_home}/
%dir %{dovecot_home}/lib/
%dir %{dovecot_home}/lib/dovecot
%{dovecot_home}/lib/dovecot/lib90_push_notification_kafka_plugin.so


##############################################################################
# changelog
##############################################################################
%changelog
