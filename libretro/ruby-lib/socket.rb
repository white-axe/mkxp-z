# Temporary replacement for the Ruby socket extension, which won't compile currently because WASI doesn't have socket support yet.
# Once WASI Preview 3 releases and is integrated into WASI libc, we can upgrade to WASI Preview 3 and use the actual socket extension instead of this.
# Licensed under CC0.

class Socket
  def initialize(*args)
    raise 'Sockets are currently unsupported'
  end

  def self.open(*args)
    new(*args)
  end
end

class TCPSocket < Socket
end

class UDPSocket < Socket
end
