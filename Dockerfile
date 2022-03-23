FROM ubuntu
COPY . /app
ENV LD_LIBRARY_PATH=/app/lib/
WORKDIR /app/bin/
ENTRYPOINT ["./server"]
#ENTRYPOINT ["./client"]
#CMD ["lyh","0.0.0.0","8888"]
