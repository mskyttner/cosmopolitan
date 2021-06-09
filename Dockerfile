FROM alpine AS builder

RUN apk update -qq && apk upgrade && apk add --no-cache \
	make \
	zip

WORKDIR /build

COPY . cosmopolitan 

RUN N_CPU=$((`nproc --all`-1)) && \
	echo "Number of CPUs: $N_CPU" && \
	cd cosmopolitan && \
	make o//tool/net/redbean.com -j$N_CPU V=0

RUN find cosmopolitan/o -type f -name "*\.com" -exec cp {} /tmp \;

FROM alpine

WORKDIR /usr/local/bin
COPY --from=builder /tmp .

WORKDIR /data
VOLUME ["/data"]
EXPOSE 8080

CMD ["redbean.com -vv"]
