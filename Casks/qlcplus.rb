cask "qlcplus" do
  version :latest
  sha256 :no_check

  url "https://github.com/abossard/qlcplus/releases/download/mcp-latest/QLC%2B_build-v5-mcp.dmg"
  name "Q Light Controller Plus"
  desc "DMX lighting control with MCP server for AI integration"
  homepage "https://github.com/abossard/qlcplus"

  app "QLC+.app"

  postflight do
    system_command "/usr/bin/xattr",
                   args: ["-cr", "#{appdir}/QLC+.app"]
  end

  caveats <<~EOS
    The app has been quarantine-cleared automatically.

    To start QLC+ with the MCP server:
      open /Applications/QLC+.app --args --mcp-http 9696

    Then configure your MCP client to connect to http://localhost:9696/mcp
  EOS
end
