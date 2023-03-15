
LOG_INC = -I../../log
LOG_LIB = -L../../log -lWebChatLog

AWS_SDK_INC = -I/usr/local/aws/sdk_release/include
AWS_SDK_LIB = -L/usr/local/aws/sdk_release/lib64 -laws-cpp-sdk-s3  -laws-cpp-sdk-core -laws-crt-cpp -laws-c-auth -laws-c-cal -laws-c-common \
-laws-c-compression -laws-c-event-stream -laws-c-http -laws-c-io -laws-c-mqtt -laws-c-s3 -laws-checksums -laws-c-sdkutils -ls2n -lcurl

SSL_LIB = -L/usr/local/openssl/lib/ -lssl -lcrypto

CFLAG = -g -w -shared -fPIC -static-libstdc++ -std=c++11 -I../server

all:
	g++  ${CFLAG} ${AWS_SDK_INC} ${OPENSSL_INC} ${LOG_INC} aws_s3.cpp wrapper.cpp -o libAws.so -Wl,-Bstatic -Wl,--start-group \
	${AWS_SDK_LIB} ${SSL_LIB} ${LOG_LIB} -Wl,--end-group -Wl,-Bdynamic -lpthread -lz -lrt

	mv libAws.so /home/jiaxu/code/aws_cpp/lib
