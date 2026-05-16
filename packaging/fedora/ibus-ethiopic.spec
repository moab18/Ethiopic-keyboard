Name:           ibus-ethiopic
Version:        0.1.0
Release:        1%{?dist}
Summary:        IBus engine for Ethiopic input

License:        GPL-3.0-or-later
URL:            https://github.com/moab/ethiopic-keyboard
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.16
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig(ibus-1.0) >= 1.5
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-2.0)

Requires:       ibus >= 1.5

%description
A two-layer Ethiopic input method for IBus: a platform-independent C++
core library (libethio) and a thin IBus engine wrapper. Supports all
Ethiopic-script languages (Amharic, Tigrinya, Oromo, Guragigna, etc.)
via SERA transliteration with auto-commit, prefix disambiguation,
word suggestions, and labiovelar forms.

%package -n libethio-devel
Summary:        Development files for libethio
Requires:       %{name} = %{version}-%{release}

%description -n libethio-devel
Headers and static library for developing applications or input method
wrappers (Windows TSF, macOS IMKit, Android, iOS) using the libethio
trie-based Ethiopic input engine.

%prep
%setup -q

%build
%cmake \
    -DCMAKE_INSTALL_LIBEXECDIR=%{_libexecdir} \
    -DCMAKE_INSTALL_LIBDIR=%{_libdir}
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%{_libexecdir}/ibus-engine-ethiopic
%{_datadir}/ibus/component/ethiopic.xml
%dir %{_datadir}/ibus-ethiopic
%{_datadir}/ibus-ethiopic/default.xml
%{_datadir}/ibus-ethiopic/amharic/

%files -n libethio-devel
%{_includedir}/ethio/
%{_libdir}/libethio.a

%changelog
* Sat May 16 2026 Moab <moab@example.com> - 0.1.0-1
- Initial Fedora package
