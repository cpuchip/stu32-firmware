# How this was built

Michael Stufflebeam owns this calculator and rules on it. The code was written by a team of AI agents, each a
Claude Code session with its own charter, working with him. The commit messages, work units and comments name these
agents (the "seats") and the messages between them. They stay in the history on purpose, so anyone can see how the
work was done and try it themselves.

## The seats

- **abacus** writes the specification and the work units (`work/`), reviews each unit, and runs the black-box
  checks against other implementations. It works on a separate machine, and only verdicts and the manuals' expected
  values cross to the author: pass or fail, never another core's output or source.
- **soroban** is the clean-room author of the core and firmware (`core/`, `firmware/`, `device/`). It works only
  from HP's published manuals, our specification, standard mathematical references and permissively licensed
  libraries. It has never had another calculator's source on its machine. It also gates every change that enters
  this repository, including other seats' code.
- **casim** writes Casimir, the computer-algebra library behind the CAS features (MIT, its own repository, fetched at
  build).
- **arcade** writes the games in `games.list` (stu32-arcade, MIT). They come in through soroban's gate like any other
  change.
- **draftsman** designs the hardware. It lives in its own repository, not here.
- **tutor**, **primer** and **tally** build the products that pin this core: the tutor, and the web and app builds.
- **basecamp** coordinates the seats and relays Michael's rulings.

## The numbers

A `#NNNN` in a commit or a comment (for example "abacus #6765") is a message in our internal message log: the
request, ruling or review that a change answers. The log itself is not public. The number shows which conversation a
line came from and in what order.

Decisions are numbered too ("decision 83"): those are Michael's rulings, recorded by abacus.

## The rules the work kept

- **Clean room:** the author reads the manuals, never another implementation.
- **Oracle first:** each function's test, usually from a worked example in a manual, exists and fails before the
  function is written. Each commit says where its behaviour came from.
- **Every unit reports its cost** (since decision 68): flash, RAM, stack and speed, measured on the boards.
- **A check that has never been seen to fail is not evidence:** gates are tested against planted faults.
