SUBDIRS=source test

package:: rpm

rpmmacros::
	@echo "%_topdir  $(PWD)/rpmbuild" > ~/.rpmmacros
	@echo "%_tmppath $(PWD)/rpmbuild/tmp" >> ~/.rpmmacros

rpmbuild_dirs:: 
	@rm -rf rpmbuild
	@mkdir rpmbuild
	@mkdir rpmbuild/BUILD
	@mkdir rpmbuild/RPMS
	@mkdir rpmbuild/SOURCES
	@mkdir rpmbuild/SPECS
	@mkdir rpmbuild/SRPMS
	@mkdir rpmbuild/tmp

rpm:: dist rpmmacros rpmbuild_dirs
	@cp @PACKAGE_NAME@*.tar.gz rpmbuild/SOURCES/
	@cp packaging/fedora/@PACKAGE_NAME@.spec rpmbuild/SPECS
	@rpmbuild -bb --nodeps ./rpmbuild/SPECS/globus-gridftp-server-hpss.spec
if GCSV5_BUILD_AVAILABLE
	@rpmbuild -bb --nodeps --define '_gcs_tag gcsv5' ./rpmbuild/SPECS/globus-gridftp-server-hpss.spec
endif
	@cp rpmbuild/RPMS/x86_64/* .

EXTRA_DIST=data
