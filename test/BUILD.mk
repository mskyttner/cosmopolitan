#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set noet ft=make ts=8 sw=8 fenc=utf-8 :vi ────────────────────┘

.PHONY:		o/$(MODE)/test
o/$(MODE)/test:	o/$(MODE)/test/dsp	\
		o/$(MODE)/test/libc	\
		o/$(MODE)/test/net	\
		o/$(MODE)/test/libcxx	\
		o/$(MODE)/test/posix	\
		o/$(MODE)/test/tool
