#include <goblin-engineer/components/http/http_server/http_session.hpp>

#include <goblin-engineer/components/http/detail/network.hpp>
#include <goblin-engineer/components/http/http_server/websocket_session.hpp>

#include <boost/beast/core/bind_handler.hpp>
#include <iostream>

namespace goblin_engineer { namespace components { namespace http_server {

inline void fail(boost::beast::error_code ec, char const *what) {
  std::cerr << what << ": " << ec.message() << "\n";
}

http_session::http_session(detail::tcp::socket &&socket,helper_write_f_t context_)
    : stream_(std::move(socket))
    , queue_(*this)
    , handle_processing(context_)
    , id(reinterpret_cast<std::uintptr_t>(this)) {}

void http_session::run() {
  boost::asio::dispatch(
      stream_.get_executor(),
      detail::beast::bind_front_handler(&http_session::do_read, this->shared_from_this())
  );
}

void http_session::on_read(boost::beast::error_code ec,std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  // This means they closed the connection
  if (ec == detail::http::error::end_of_stream)
    return do_close();

  if (ec)
    return fail(ec, "read");

  // See if it is a WebSocket Upgrade
  if (detail::websocket::is_upgrade(parser_->get())) {
    // Create a websocket session, transferring ownership
    // of both the socket and the HTTP request.
    std::make_shared<websocket_session>(stream_.release_socket(),handle_processing)->do_accept(parser_->release());
    return;
  }

  // Send the response
  handle_processing(parser_->release(), id);

  // If we aren't at the queue limit, try to pipeline another request
  if (!queue_.is_full())
    do_read();
}

void http_session::on_write(
    bool close
    , boost::beast::error_code ec
    , std::size_t bytes_transferred ) {
  boost::ignore_unused(bytes_transferred);

  if (ec)
    return fail(ec, "write");

  if (close) {
    // This means we should close the connection, usually because
    // the response indicated the "Connection: close" semantic.
    return do_close();
  }

  // Inform the queue that a write completed
  if (queue_.on_write()) {
    // Read another request
    do_read();
  }
}

void http_session::do_close() {
  boost::beast::error_code ec;
  stream_.socket().shutdown(detail::tcp::socket::shutdown_send, ec);
}

void http_session::do_read() {
  // Construct a new parser for each message
  parser_.emplace();

  // Apply a reasonable limit to the allowed size
  // of the body in bytes to prevent abuse.
  parser_->body_limit(10000);

  // Set the timeout.
  stream_.expires_after(std::chrono::seconds(30));

  // Read a request using the parser-oriented interface
  detail::http::async_read(
      stream_
      , buffer_
      , *parser_
      ,boost::beast::bind_front_handler(
          &http_session::on_read
          , shared_from_this()
      )
  );
}

void http_session::write(detail::response_type &&body) {
  boost::asio::post(
      stream_.get_executor(),
      [this, body = std::move(body)]() mutable {
        queue_(std::move(body));
      }
  );
}

http_session::queue::queue(http_session &self) : self_(self) {
  static_assert(limit > 0, "queue limit must be positive");
  items_.reserve(limit);
}

bool http_session::queue::on_write() {
  assert(!items_.empty());
  auto const was_full = is_full();
  items_.erase(items_.begin());
  if (!items_.empty())
    (*items_.front())();
  return was_full;
}
}}}