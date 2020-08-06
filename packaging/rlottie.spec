%define         major 0
%define         libname %mklibname rlottie %{major}
%define         devel %mklibname rlottie -d

%define         git 20200408
%define         rel 1

Name:           rlottie
Version:        0.0.1
Release:        %mkrel %{?git:-c git%{git}} %{rel}
Summary:        Platform independent standalone library that plays Lottie Animation
Group:          System/Libraries
License:        MIT and BSD
URL:            http://www.tizen.org/
Source0:        https://github.com/Samsung/rlottie/archive/master/%{name}-%{version}%{?git:-git%{git}}.tar.gz

BuildRequires:  meson

%description
rlottie is a platform independent standalone c++ library for rendering vector
based animations and art in realtime.

Lottie loads and renders animations and vectors exported in the bodymovin JSON
format. Bodymovin JSON can be created and exported from After Effects with
bodymovin, Sketch with Lottie Sketch Export, and from Haiku.

For the first time, designers can create and ship beautiful animations without
an engineer painstakingly recreating it by hand. Since the animation is backed
by JSON they are extremely small in size but can be large in complexity!

%package -n %{devel}
Summary:       Development libraries for rlottie
Group:         System/Libraries

%description -n %{devel}
rlottie is a platform independent standalone c++ library for rendering vector
based animations and art in realtime.

Lottie loads and renders animations and vectors exported in the bodymovin JSON
format. Bodymovin JSON can be created and exported from After Effects with
bodymovin, Sketch with Lottie Sketch Export, and from Haiku.

For the first time, designers can create and ship beautiful animations without
an engineer painstakingly recreating it by hand. Since the animation is backed
by JSON they are extremely small in size but can be large in complexity!                                                                                                                                                                                                        
                                                                                                                                                                                                                                                                                
This is the development libraries for rlottie.                                                                                                                                                                                                                                  
                                                                                                                                                                                                                                                                                
%package -n %{libname}                                                                                                                                                                                                                                                          
Summary:       Platform independent standalone library that plays Lottie Animation                                                                                                                                                                                              
Group:         System/Libraries                                                                                                                                                                                                                                                 
                                                                                                                                                                                                                                                                                
%description -n %{libname}                                                                                                                                                                                                                                                      
rlottie is a platform independent standalone c++ library for rendering vector                                                                                                                                                                                                   
based animations and art in realtime.                                                                                                                                                                                                                                           
                                                                                                                                                                                                                                                                                
Lottie loads and renders animations and vectors exported in the bodymovin JSON                                                                                                                                                                                                  
format. Bodymovin JSON can be created and exported from After Effects with                                                                                                                                                                                                      
bodymovin, Sketch with Lottie Sketch Export, and from Haiku.                                                                                                                                                                                                                    
                                                                                                                                                                                                                                                                                
For the first time, designers can create and ship beautiful animations without                                                                                                                                                                                                  
an engineer painstakingly recreating it by hand. Since the animation is backed                                                                                                                                                                                                  
by JSON they are extremely small in size but can be large in complexity!                                                                                                                                                                                                        
                                                                                                                                                                                                                                                                                
%prep                                                                                                                                                                                                                                                                           
%autosetup -n %{name}-master                                                                                                                                                                                                                                                    
                                                                                                                                                                                                                                                                                
%build                                                                                                                                                                                                                                                                          
%meson                                                                                                                                                                                                                                                                          
                                                                                                                                                                                                                                                                                
%install                                                                                                                                                                                                                                                                        
%meson_install

%files -n %{devel}                                                                                                                                                                                                                                                              
%{_includedir}/rlottie.h
%{_includedir}/rlottie_capi.h
%{_includedir}/rlottiecommon.h
%{_libdir}/pkgconfig/rlottie.pc
%{_libdir}/librlottie.so

%files -n %{libname}
%license COPYING
%doc README.md AUTHORS
%{_libdir}/librlottie-image-loader.so
%{_libdir}/librlottie.so.%{major}{,.*}
