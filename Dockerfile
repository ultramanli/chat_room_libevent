FROM ubuntu
COPY . /app
ENV LD_LIBRARY_PATH=/app/lib/
WORKDIR /app/bin/
ENTRYPOINT ["./sever"]
#CMD ["lyh","0.0.0.0","8888"]
