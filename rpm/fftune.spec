Name:       fftune
Summary:    Command line tool for testing force feedback effects
Version:    0.5
Release:    1
Group:      Development/Tools/Other
License:    GPL 2
URL:        https://bitbucket.org/jolla/tools-fftune
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(kernel-headers)

%description
fftune is a command line tool for creating and running force feedback effects
for devices that support standard ff-memless driver effects. It will work also
on normal force feedback devices that support FF_RUMBLE or FF_PERIODIC type
of effects.

%prep
%setup -q -n %{name}-%{version}

%build

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

%files
%defattr(755,root,root,-)
%{_bindir}/*




