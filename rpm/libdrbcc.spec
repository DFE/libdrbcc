#
# spec file for package libdrbcc
#

%define _SO_nr 0

Name:           libdrbcc
Version:        git
Release:        0
Summary:        DResearch Board Controller Communication Library
License:        LGPL-3.0
Group:          System/Libraries
Url:            https://github.com/DFE/%{name}
Source:         %{name}-%{version}.tar.gz
Requires:       readline
BuildRequires:  readline-devel

%description
Libdrbcc is a library implementing the DResearch Board Controller Communication
protocol used in HydraIP devices.

%package -n libdrbcc%{_SO_nr}
Summary:        DResearch Board Controller Communication Library
Group:          System/Libraries

%description -n libdrbcc%{_SO_nr}
Libdrbcc is a library implementing the DResearch Board Controller Communication
protocol used in HydraIP devices.

%package -n drbcc
Summary:        CLI for DResearch Board Controller Communication
License:        GPL-3.0
Group:          Tools/Other
Requires:       libdrbcc%{_SO_nr} = %{version}

%description  -n drbcc
This packages contains the command line client for the DResearch Board Controller
Communication protocol.

%package devel
Summary:        DResearch Board Controller Communication Library -- Development Files
Group:          Development/Libraries/C and C++
Requires:       libdrbcc%{_SO_nr} = %{version}, readline-devel

%description devel
This package contains all necessary include files and libraries needed
to compile and develop applications that use libdrbcc.

%prep
%setup -q -n %{name}-%{version}

%build
%configure  --disable-static
make

%install
%make_install
# remove unneeded files
rm -f %{buildroot}%{_libdir}/*.la

%post -n libdrbcc%{_SO_nr} -p /sbin/ldconfig

%postun -n libdrbcc%{_SO_nr} -p /sbin/ldconfig

%files -n libdrbcc%{_SO_nr}
%defattr (-, root, root)
%doc COPYING.LIB README.md
%{_libdir}/libdrbcc.so.%{_SO_nr}*

%files -n drbcc
%defattr (-, root, root)
%doc COPYING
%{_bindir}/drbcc

%files devel
%defattr (-, root, root)
%{_includedir}/drbcc*.h
%{_libdir}/libdrbcc.so

%changelog
* Thu Nov 27 2014 sledz@dresearch-fe.de
- initial version
