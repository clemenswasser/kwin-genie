Name:           kwin-genie
Version:        1.0.0
Release:        1%{?dist}
Summary:        macOS-style genie minimize effect for KWin
License:        GPL-2.0-or-later
URL:            https://github.com/clemenswasser/kwin-genie
Source0:        %{url}/archive/v%{version}/kwin-genie-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  extra-cmake-modules
BuildRequires:  kwin-devel
BuildRequires:  qt6-qtbase-devel
BuildRequires:  kf6-kcoreaddons-devel

Requires:       kwin
Requires:       qt6-qtbase
Requires:       kf6-kcoreaddons

%description
A native KWin effect reproducing the macOS genie minimize and restore animation.

%prep
%autosetup -n kwin-genie-%{version}

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md PROVENANCE.md
%{_libdir}/qt6/plugins/kwin/effects/plugins/genie.so

%changelog
* Tue Nov 10 2026 Clemens Wasser <clemens@wassers.me> - 1.0.0-1
- Initial package
