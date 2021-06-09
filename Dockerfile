FROM alpine AS builder

RUN apk update -qq && apk upgrade && apk add --no-cache \
	build-base \
	cmake clang clang-dev make gcc g++ libc-dev linux-headers \
	bash \
	unzip \
	curl

WORKDIR /build

# error: make: *** [tool/net/net.mk:81: o//tool/net/redbean.com] Error 127
#RUN curl -o cosmo.tgz "https://justine.lol/cosmopolitan/cosmopolitan-1.0.tar.gz" && \
#	tar xf cosmo.tgz && \
#	rm cosmo.tgz

# error: Segmentation fault (core dumped)/clang.sh
#10 7.017 clang 9 EXITED WITH 139: test/libc/release/clang.sh -Wno-unused-command-line-argument -Wno-incompatible-pointer-types-discards-qualifiers -no-canonical-prefixes -fdiagnostics-color=always
COPY . cosmopolitan 

RUN N_CPU=$((`nproc --all`-1)) && \
	echo "Number of CPUs: $N_CPU" && \
	cd cosmopolitan && \
	make -j$N_CPU V=0

RUN find o -type f -name "*.com" -exec cp {} /tmp/dist \;

FROM alpine

# probably not needed?
#RUN apk update -qq && apk add --no-cache libstdc++

WORKDIR /usr/local/bin
COPY --from=builder /tmp/dist .
RUN chmod +x *

WORKDIR /data
VOLUME ["/data"]
EXPOSE 8080

CMD ["redbean.com -vv"]

