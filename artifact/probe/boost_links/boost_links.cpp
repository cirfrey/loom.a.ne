#include <boost/asio.hpp>
#include <boost/fiber/all.hpp>
#include <boost/context/fiber.hpp>

int main() {
    boost::asio::io_context io;
    boost::fibers::fiber f([]{}); f.join();
}
