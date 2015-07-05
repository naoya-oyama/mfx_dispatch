Summary: 	Intel Media SDK Dispatched static library
Name: 		libmfx
Version: 	1.7.0
Release: 	1%{?dist}
License: 	BSD
Group: 		System Environment/Libraries
URL: 		https://github.com/lu-zero/mfx_dispatch
Source0: 	https://github.com/naoya-oyama/mfx_dispatch/archive/master.zip
BuildRoot: 	%{_tmppath}/%{name}-%{version}-%{release}-buildroot

%package devel
Summary:	Development files needed for libmfx
Group:		Development/Libraries
Requires:	%{name} = %{version}-%{release}
BuildRequires:  libdrm-devel
BuildRequires:  libva-devel
BuildRequires:  autoconf

#---------------------------------------------------------------------

%description
libmfx is a free library for using the Intel Media SDK library.

%description devel
libmfx is a free library for using the Intel Media SDK library.
This package contains development files for libmfx.

#---------------------------------------------------------------------

%prep
%setup -q -n mfx_dispatch-master
  autoreconf -i

#---------------------------------------------------------------------

%build
%configure
make %{?_smp_mflags}

#---------------------------------------------------------------------

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=%{buildroot}

#---------------------------------------------------------------------

%clean
rm -rf $RPM_BUILD_ROOT

#---------------------------------------------------------------------

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%exclude %{_libdir}/libmfx.la
%doc COPYING
%{_libdir}/libmfx.so*

%files devel
%defattr(-,root,root,-)
%{_libdir}/libmfx.a
%{_libdir}/pkgconfig/libmfx.pc
%{_includedir}/mfx/*

#---------------------------------------------------------------------

%changelog
* Wed Jul 1 2015 Naoya OYAMA <naoya.oyama[at]gmail.com>
- Initial build.
