FROM gcc:13
WORKDIR /app
COPY server.cpp .
RUN g++ -o server server.cpp -lpthread -std=c++17 -O2
EXPOSE 9090
CMD ["./server"]