import sys
import importlib
import json
sys.path.insert(1,"/workspaces/whisk-controller-dev/action-runtimes/python3/functions")

if __name__ == "__main__":
    main = importlib.import_module(sys.argv[1]).main
    # Notify the Controller that the python runtime is ready for execution.
    sys.stdout.write("\0")
    sys.stdout.flush()

    params = json.loads(sys.stdin.readline())
    result = json.dumps(
        main(params)
    )

    sys.stdout.write(result)
    sys.stdout.flush()