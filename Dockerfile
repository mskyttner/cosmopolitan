FROM alpine AS builder

RUN apk update -qq && apk upgrade && apk add --no-cache \
	make \
	zip

WORKDIR /build

COPY . cosmopolitan 

RUN N_CPU=$((`nproc --all`)) && \
	echo "Number of CPUs: $N_CPU\n" && \
	cd cosmopolitan && \
	make o//tool/net/redbean.com -j$N_CPU V=0

RUN find cosmopolitan/o -type f -name "*\.com" -exec cp {} /tmp \;

FROM alpine

WORKDIR /usr/local/bin
COPY --from=builder /tmp .

WORKDIR /data
VOLUME ["/data"]
EXPOSE 8080

CMD sh -c redbean.com -vv -D /data

# usage 

# serve files in working directory: 
# docker run --rm -p 8080:8080 -v $(pwd):/data ${IMAGE_SLUG}

# get root shell on container
# docker run --rm -it -p 8080:8080 -v $(pwd):/data ${IMAGE_SLUG} ash

# run sqlite3 w database my.db in current dir
# docker run --rm -it v $(pwd):/data ${IMAGE_SLUG} sh -c 'sqlite3.com /data/my.db'
