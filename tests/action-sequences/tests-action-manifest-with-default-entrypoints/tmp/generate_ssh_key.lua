local function generate_ssh_keys(params)
  os.execute("ssh-keygen -q -f /tmp/generated-key -t rsa -b 2048 -C kcexton01 -N ''")
  pub = io.open("/tmp/generated-key.pub", "r")
  public_key = pub:read()
  priv = io.open("/tmp/generated-key", "r")
  private_key = priv:read("a")
  ssh_keys = {
    ["public_key"]=public_key,
    ["private_key"]=private_key
  }
  pub:close()
  priv:close()
  os.remove("/tmp/generated-key")
  os.remove("/tmp/generated-key.pub")
  return ssh_keys
end

return {
  main = generate_ssh_keys
}
