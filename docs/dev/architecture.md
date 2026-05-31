# Project Architecture

Working notes on how coconext relates to cocotb and where it sits in the
simulator/GPI stack.

## cocotb today

cocotb wraps the simulator in successive layers, terminating in a Python test
written by the user.

```mermaid
flowchart TB
    subgraph py_user["Python userland"]
        test[cocotb test]
    end
    subgraph py_lib["Python library"]
        rt[cocotb runtime]
    end
    subgraph cocotb_cpp["cocotb C++ library"]
        pygpi[PyGPI]
    end
    subgraph shared_cpp["Shared C++ library"]
        gpi[GPI]
    end
    subgraph third["Third-party"]
        sim[Simulator]
    end
    test --> rt
    rt --> pygpi
    pygpi --> gpi
    gpi --> sim

    classDef tp fill:#f4d6cc,stroke:#b85c3c,color:#000
    classDef cpp fill:#cfe2f3,stroke:#3d6fa5,color:#000
    classDef py fill:#d9ead3,stroke:#5b8a4a,color:#000
    classDef user fill:#fff2cc,stroke:#b08a2e,color:#000
    class sim tp
    class gpi,pygpi cpp
    class rt py
    class test user
```

Layers, bottom to top:

- **Simulator** (third-party). Provides VPI/VHPI/FLI.
- **GPI** (shared C++ library). Normalizes the simulator-specific APIs into a
  single C++ API. Owned by cocotb today, but not Python-specific; intended to
  be the common base for any wrapper.
- **PyGPI** (cocotb C++ library). Embeds CPython and exposes GPI to Python.
- **cocotb runtime** (Python). Scheduler, handles, triggers, decorators.
- **cocotb test** (Python userland). What the user actually writes.

## coconext

coconext replaces the PyGPI + cocotb-runtime stack with a single C++ library.
The user writes a C++ testbench directly against it.

```mermaid
flowchart TB
    subgraph cpp_user["C++ userland"]
        tb[C++ testbench]
    end
    subgraph coconext_cpp["coconext C++ library"]
        coconext[coconext]
    end
    subgraph shared_cpp["Shared C++ library"]
        gpi[GPI]
    end
    subgraph third["Third-party"]
        sim[Simulator]
    end
    tb --> coconext
    coconext --> gpi
    gpi --> sim

    classDef tp fill:#f4d6cc,stroke:#b85c3c,color:#000
    classDef cpp fill:#cfe2f3,stroke:#3d6fa5,color:#000
    classDef user fill:#fff2cc,stroke:#b08a2e,color:#000
    class sim tp
    class gpi,coconext cpp
    class tb user
```

Layers, bottom to top:

- **Simulator** (third-party). Unchanged.
- **GPI** (shared C++ library). Unchanged; same library used by PyGPI.
- **coconext** (coconext C++ library). Subsumes the role of PyGPI + cocotb
  runtime: handle wrappers, scheduler, triggers, test-harness primitives.
- **C++ testbench** (C++ userland). What the user writes.

## cocotb and coconext together

Both stacks share GPI underneath. A binding library exposes coconext to the
cocotb runtime so existing Python tests can drive coconext-based components,
and Python user code can additionally call into the user's own C++ code via
Python bindings.

```mermaid
flowchart TB
    subgraph py_user["Python userland"]
        py_test[Python user code / cocotb test]
    end
    subgraph cpp_user["C++ userland"]
        cpp_tb[C++ user code]
        cpp_bind[Python bindings for C++ user code]
    end
    subgraph py_lib["Python library"]
        rt[cocotb runtime]
    end
    subgraph cocotb_cpp["cocotb C++ library"]
        pygpi[PyGPI]
    end
    subgraph coconext_cpp["coconext C++ library"]
        cn_bind[cocotb-coconext binding]
        coconext[coconext]
    end
    subgraph shared_cpp["Shared C++ library"]
        gpi[GPI]
    end
    subgraph third["Third-party"]
        sim[Simulator]
    end

    py_test --> rt
    py_test --> cpp_bind
    cpp_bind --> cpp_tb
    cpp_tb --> coconext
    rt --> pygpi
    rt --> cn_bind
    cn_bind --> coconext
    pygpi --> gpi
    coconext --> gpi
    gpi --> sim

    classDef tp fill:#f4d6cc,stroke:#b85c3c,color:#000
    classDef cpp fill:#cfe2f3,stroke:#3d6fa5,color:#000
    classDef py fill:#d9ead3,stroke:#5b8a4a,color:#000
    classDef user fill:#fff2cc,stroke:#b08a2e,color:#000
    class sim tp
    class gpi,pygpi,coconext,cn_bind cpp
    class rt py
    class py_test,cpp_tb,cpp_bind user
```

Key paths:

- **Pure cocotb path**: `cocotb test -> cocotb runtime -> PyGPI / coconext via bindings -> GPI`.
- **Pure coconext path**: `C++ testbench -> coconext -> GPI`.
- **Mixed path (Python driving coconext)**: `cocotb test -> cocotb runtime ->
  cocotb-coconext binding -> coconext -> GPI`.
- **Mixed path (Python calling user C++)**: `Python user code -> Python
  bindings -> user's C++ -> coconext -> GPI`.

PyGPI and coconext are siblings over GPI; the only coupling between them is
through the optional binding shim, so a pure-C++ deployment doesn't pull in
CPython.

coconext also provides a patcher so in this mode parts of the cocotb runtime
are overwritten with coconext implementations (exposed via nanobind) to
1. Improve general cocotb performance slightly.
2. Improve performance and features of Python test code that shares coconext
   objects with C++ test code.
