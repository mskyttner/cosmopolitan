FROM alpine AS builder

RUN apk update -qq && apk upgrade && apk add --no-cache \
	make \
	zip

WORKDIR /build

COPY . cosmopolitan 

RUN N_CPU=$((`nproc --all`)) && \
	echo "Number of CPUs: $N_CPU\n" && \
	cd cosmopolitan && \
	make MODE=rel o//tool/net/redbean.com -j$N_CPU V=0 && \
	make MODE=rel o//third_party/sqlite3 -j$N_CPU V=0

RUN find cosmopolitan/o -type f -name "*\.com" -exec cp {} /tmp \;

FROM alpine

WORKDIR /usr/local/bin
COPY --from=builder /tmp .

WORKDIR /data

EXPOSE 8080

CMD sh -c cd /data && redbean.com -vvmbag -D .

# usage 

# serve files in working directory under app: 
# export IMAGE_SLUG=cosmo # or what you named the image
# docker run --rm -p 8080:8080 -v $(pwd):/data/app ${IMAGE_SLUG}
# curl -L localhost:8080/app

# run sqlite3 w database cosmosources.db in app dir
# docker run --rm -it -v $(pwd):/data/app ${IMAGE_SLUG} sh -c 'sqlite3.com app/cosmosources.db'

# get root shell on container
# docker run --rm -it -p 8080:8080 -v $(pwd):/data/app ${IMAGE_SLUG} ash


