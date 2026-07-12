#ifndef COCONEXT_CANCELLED_HPP
#define COCONEXT_CANCELLED_HPP

#include <string>
#include <string_view>

namespace coconext {

class Cancelled : public std::exception {
  public:
    Cancelled() noexcept : msg_("") {}
    Cancelled(std::string_view message) : msg_(message) {}

    char const* what() const noexcept override { return msg_.c_str(); }

  private:
    std::string msg_;
};

}  // namespace coconext

#endif  // COCONEXT_CANCELLED_HPP
