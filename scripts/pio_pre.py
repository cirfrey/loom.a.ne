Import("env")

# Prevent tinyusb from complaining about some internal stuff
# that's none of my business.
env.Append(CXXFLAGS=["-Wno-volatile"])
