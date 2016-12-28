//
// This file is part of ssh-adverb.
// Copyright (C) 2015 Richard Kettlewell
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include <config.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

// Shell-quote a collection of words
class Quoter {
  std::stringstream ss;         // accumulated quoted command
  bool quoted;                  // current quoting state

  // Force the quoting state to be whatever is required
  void set_quoted(bool state) {
    if(state != quoted) {
      ss << '\'';
      quoted = !quoted;
    }
  }

  // Add one character
  void add_char(char c) {
    if(c == '\'') {
      set_quoted(false);
      ss << "\\'";
    } else {
      set_quoted(true);
      ss << c;
    }
  }

public:
  // Add a whole word
  void add_word(const std::string &w) {
    ss << ' ';
    quoted = false;
    for(size_t i = 0; i < w.size(); ++i)
      add_char(w.at(i));
    set_quoted(false);
  }

  // Retrieve the full quoted string
  std::string str() const {
    return ss.str();
  }
};

// Table of known SSH options
static const struct option {
  char letter;
  const char *arg;
  const char *description;
} options[] = {
  { '1', NULL, "Use protocol version 1 only" },
  { '2', NULL, "Use protocol version 2 only" },
  { '4', NULL, "Use IPv4 addresses only" },
  { '6', NULL, "Use IPv6 addresses only" },
  { 'A', NULL, "Forward authentication agent connection" },
  { 'a', NULL, "Do not forward authentication agent connection" },
  { 'b', "BIND-ADDRESS", "Set local socket address" },
  { 'C', NULL, "Enable compression" },
  { 'c', "CIPHER-SPEC", "Set acceptable cipher(s)" },
  { 'D', "[BIND_ADDRESS:]PORT",
         "Local dynamic application-level (SOCKS) port forward" },
  { 'e', "ESCAPE-CHAR", "Set escape character" },
  { 'F', "CONFIG-FILE", "Set configuration file" },
  { 'f', NULL, "Go into background before executing command" },
  { 'g', NULL, "Allow remote hosts to connect to local forwarded ports" },
  { 'I', "PKCS11", "Set PKCS#11 provider" },
  { 'i', "IDENTITY-FILE", "Set private key file" },
  { 'K', NULL, "Enable GSSAPI authentication and forwarding" },
  { 'k', NULL, "Disable GSSAPI forward" },
  { 'L', "[BIND-ADDRESS:]PORT:HOST:HOSTPORT",
         "Forward local PORT via server to HOST:HOSTPORT" },
  { 'l', "LOGIN-NAME", "Set login name" },
  { 'M', NULL, "Set master mode" },
  { 'm', "MAC-SPEC", "Set acceptable MAC(s)" },
  { 'N', NULL, "Disable remote command execution" },
  { 'n', NULL, "Disable stdin" },
  { 'O', "check|forward|cancel|exit|stop",
         "Send command to master process" },
  { 'o', "OPTION=VALUE", "Set an ssh_config option" },
  { 'p', "PORT", "Set remote port number" },
  { 'q', NULL, "Quiet mode" },
  { 'R', "[BIND-ADDRESS:]PORT:HOST:HOSTPORT",
         "Forward remote PORT via client to HOST:HOSTPORT" },
  { 'S', "CTL-PATH", "Set control socket location" },
  { 's', NULL, "Execute subsystem instead of command" },
  { 'T', NULL, "Disable psuedo-tty allocation" },
  { 't', NULL, "Force psuedo-tty allocation" },
  { 'V', NULL, "Disable version and exit" },
  { 'v', NULL, "Set verbose mode" },
  { 'W', "HOST:PORT", "Forward client stdin/out to HOST:PORT" },
  { 'w', "LOCAL-TUN[:REMOTE_TUN]", "tun(4) forwarding" },
  { 'X', NULL, "Enable X11 forwarding" },
  { 'x', NULL, "Disable X11 forwarding" },
  { 'Y', NULL, "Enable trusted X11 forwarding" },
  { 'y', NULL, "Log to syslog instead of stderr" },
};

// Find an option given its letter
static const struct option *find_option(char letter) {
  for(size_t i = 0; i < sizeof options / sizeof *options; ++i) {
    if(options[i].letter == letter)
      return &options[i];
  }
  throw std::runtime_error("unrecognized option -" + std::string(1, letter));
}

// Display usage message
static void help() {
  std::cout << "Usage:\n"
            << "  ssh-adverb [OPTIONS] [--] HOST CMD ARG...\n"
            << "Options:\n";
  size_t max_option = 0;
  for(size_t i = 0; i < sizeof options / sizeof *options; ++i) {
    if(options[i].arg)
      max_option = std::min(std::max(max_option, strlen(options[i].arg)),
                            static_cast<size_t>(14));
  }
  for(size_t i = 0; i < sizeof options / sizeof *options; ++i) {
    std::cout << "  -" << options[i].letter << ' ';
    size_t pad = max_option;
    if(options[i].arg) {
      std::cout << options[i].arg;
      if(strlen(options[i].arg) <= max_option)
        pad = max_option - strlen(options[i].arg);
      else
        std::cout << "\n     ";
    }
    while(pad-- > 0)
      std::cout << ' ';
    std::cout << "  " << options[i].description << '\n';
  }
}

int main(int argc, char **argv) {
  try {
    std::vector<const char *> cmdv;
    int n;
    const std::string sep = "--";
    const std::string version_arg = "--version";
    const std::string help_arg = "--help";
    const char *ssh_command = "ssh", *e;

    if((e = getenv("SSH_ADVERB_CMD")))
      ssh_command = e;

    cmdv.push_back(ssh_command);

    // Collect options
    for(n = 1; n < argc && argv[n][0] == '-' && argv[n] != sep; ++n) {
      // Options unique to ssh-adverb
      if(argv[n] == version_arg) {
        std::cout << "ssh-adverb " VERSION "\n";
        return 0;
      } else if(argv[n] == help_arg) {
        help();
        return 0;
      }
      // SSH options
      cmdv.push_back(argv[n]);
      for(const char *opt = argv[n] + 1; *opt; ++opt) {
        const option *o = find_option(*opt);
        if(o->arg) {
          if(opt[1])
            break; // rest of argv[n] is the argument
          else
            if(n + 1 < argc)
              cmdv.push_back(argv[++n]); // the next option is argument
        }
      }
    }

    // Skip separator if present
    if(n < argc && argv[n] == sep)
      ++n;

    // Must be a host
    if(n == argc)
      throw std::runtime_error("no host specified");
    if(argv[n][0] == '-')
      cmdv.push_back("--");
    cmdv.push_back(argv[n++]);

    // Skip separator if present
    if(n < argc && argv[n] == sep)
      ++n;

    // Must be a command
    if(n == argc)
      throw std::runtime_error("no command specified");

    // Shell-quote the command
    Quoter q;
    for(; n < argc; ++n)
      q.add_word(argv[n]);
    const std::string quoted = q.str();
    if(quoted.at(0) == '-')
      cmdv.push_back("--");
    cmdv.push_back(quoted.c_str());

    // execvp needs a trailing null pointer
    cmdv.push_back((char *)0);

    // Execute the command
    execvp(cmdv[0], const_cast<char *const *>(&cmdv[0]));

    // Report the error
    throw std::runtime_error(std::string("executing ") + cmdv[0]
                             + ": " + strerror(errno));
  } catch(std::runtime_error &e) {
    std::cerr << "ERROR: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
