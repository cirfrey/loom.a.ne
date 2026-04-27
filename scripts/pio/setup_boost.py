import os
import sys
Import("env")

def configure_boost(env):
    env.Append(LIBS=["boost_fiber", "boost_context", "pthread"])

    if sys.platform == "darwin":
        brew_prefix = "/opt/homebrew" if os.path.exists("/opt/homebrew") else "/usr/local"
        env.Append(CPPPATH=[f"{brew_prefix}/include"])
        env.Append(LIBPATH=[f"{brew_prefix}/lib"])

    elif sys.platform == "win32":
        if "BOOST_ROOT" in os.environ:
            env.Append(CPPPATH=[os.path.join(os.environ["BOOST_ROOT"], "include")])
            env.Append(LIBPATH=[os.path.join(os.environ["BOOST_ROOT"], "lib")])

configure_boost(env)
