class Qlcplus < Formula
  desc "DMX lighting control with MCP server for AI integration"
  homepage "https://github.com/abossard/qlcplus"
  license "Apache-2.0"
  version "pre"

  on_macos do
    url "https://github.com/abossard/qlcplus/releases/download/mcp-latest/qlcplus-pre-macos-arm64.tar.gz"
    sha256 "0d8c6221af6bcc13a83fe75112faaa0124d6b214d12e7601a9373a2c1305bd9c" # macos-arm64
  end

  def install
    prefix.install "QLC+.app"
  end

  def post_install
    system "/usr/bin/xattr", "-cr", "#{prefix}/QLC+.app"
    ln_sf "#{prefix}/QLC+.app", "/Applications/QLC+.app" if Dir.exist?("/Applications")
  end

  def caveats
    <<~EOS
      QLC+.app has been linked to /Applications.

      To start with the MCP server:
        open /Applications/QLC+.app --args --mcp-http 9696

      Then configure your MCP client to connect to:
        http://localhost:9696/mcp
    EOS
  end
end
