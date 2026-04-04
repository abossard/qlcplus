class Qlcplus < Formula
  desc "DMX lighting control with MCP server for AI integration"
  homepage "https://github.com/abossard/qlcplus"
  license "Apache-2.0"
  version "pre"

  on_macos do
    url "https://github.com/abossard/qlcplus/releases/download/mcp-latest/qlcplus-pre-macos-arm64.tar.gz"
    sha256 "b7439e020be8e72ea094234935b960400915bd7884589f20a1227a682b2f553b" # macos-arm64
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
