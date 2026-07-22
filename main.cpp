#include <boost/asio.hpp>
#include <memory>

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() { read(); }

private:
    void read() {
        auto self = shared_from_this();
        socket_.async_read_some(boost::asio::buffer(buf_),
            [this, self](boost::system::error_code ec, std::size_t n) {
                if (!ec) write(n);   // 에코
            });
    }

    void write(std::size_t n) {
        auto self = shared_from_this();
        boost::asio::async_write(socket_, boost::asio::buffer(buf_, n),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) read();
            });
    }

    tcp::socket socket_;
    std::array<char, 1024> buf_;
};

class Server {
public:
    Server(boost::asio::io_context& io, unsigned short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
        accept();
    }

private:
    void accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) std::make_shared<Session>(std::move(socket))->start();
                accept();
            });
    }

    tcp::acceptor acceptor_;
};

int main() {
    boost::asio::io_context io;
    Server server(io, 8080);
    io.run();   // 이벤트 루프
}