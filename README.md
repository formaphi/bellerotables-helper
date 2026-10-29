# bellerotables-helper

Helper C++23 do firewall da Bellerophon NGFW: um binário **one-shot**, ativado por socket do systemd, que
aplica regras iptables, **lê o estado real do kernel de volta** e devolve esse estado ao `bellero-agent`. Em
seguida encerra. Roda apenas com `CAP_NET_ADMIN`, sem root completo, e não acessa banco de dados nem rede.

Substitui o uso de cli da iptables e criação de regras manualmente. O alvo
é a imagem Yocto `meta-bellerophon` (Scarthgap, kernel 6.6). O desenvolvimento acontece em um sandbox Docker
(Ubuntu 24.04).

A regra que não pode ser quebrada: o `reported` vem da **realidade** (leitura do kernel), nunca é eco do
`desired`. Se a aplicação for parcial, o `reported` mostra exatamente o que pegou.

## Fluxo de trabalho

```
bellero-agent  ──(Unix socket)──▶  systemd  ──(sd_listen_fds)──▶  bellerotables-helper
                                                                      │ aplica via libiptc
                                                                      ▼ lê o kernel de volta
bellero-agent  ◀──────────────────  resposta (fase 2)  ───────────────┘
```

1. O systemd escuta em `/run/bellerophon/bellerotables-helper.sock` (`Accept=no`, modo `0660`, grupo do agent)
   e inicia o helper quando chega uma conexão.
2. O helper pega o socket de escuta com `sd_listen_fds()` e faz `accept()` de **exatamente uma** conexão.
   As demais ficam no backlog para a próxima ativação.
3. Confere o processo do outro lado com `SO_PEERCRED`: só o usuário do agent é aceito. O usuário vem de
   `/etc/bellerophon/bellerotables-helper.conf` (`agent_user=bellero-agent`); arquivo ausente ou usuário
   desconhecido recusa toda conexão.
4. Lê um frame: 4 bytes de tamanho (big-endian) seguidos do payload. Frames acima de 64 KiB são recusados antes
   de ler o payload. O payload é o JSON canônico do protobuf `{correlation_id, id, desired}`.
5. Aplica, relê o estado do kernel e encerra. Fase 1: sem resposta, fecha após tratar. Fase 2: um frame de
   resposta na mesma conexão, depois fecha.

### Headers POSIX e do sistema usados

A biblioteca `adapter_socket/` concentra as chamadas de sistema, com fds RAII e `errno` convertido em exceção:

| Header | Para quê |
|--------|----------|
| `<systemd/sd-daemon.h>` | `sd_listen_fds()`: recebe o socket de escuta do systemd |
| `<sys/socket.h>` | `accept4`, `recv`, `send`, `getsockopt` (`SO_PEERCRED`) |
| `<poll.h>` | espera com prazo total de I/O em milissegundos |
| `<unistd.h>`, `<fcntl.h>` | `close`, `fcntl` (flags do fd) |
| `<pwd.h>` | `getpwnam_r`: resolve o usuário do agent |

Por que um stream com tamanho no início, e não UDP ou ZeroMQ: [docs/socket-stream-and-framing.md](docs/socket-stream-and-framing.md).
Papéis de servidor e cliente: [docs/socket-roles.md](docs/socket-roles.md). Entrada única da biblioteca
(`acceptAuthorized()`): [docs/socket-endpoint.md](docs/socket-endpoint.md).

## Dependências

| Dependência | Origem | Uso |
|-------------|--------|-----|
| C++23, CMake 3.28+, GCC 13 | sandbox | mesma versão major do Yocto Scarthgap |
| Conan 2.x | sandbox | gerencia as dependências abaixo |
| `nlohmann_json` 3.11.3 | Conan | JSON dos payloads |
| `gtest` 1.14.0 (GTest + GMock) | Conan | testes |
| `libiptc-dev` (link `ip4tc`), `libxtables-dev` | apt | aplicar e ler regras |
| `libsystemd-dev` | apt | ativação por socket |
| `iptables` no backend legacy | apt | a CLI enxerga o que a libiptc escreve |

Adicionar uma dependência Conan exige `make sandbox-build` de novo: a imagem pré-compila o cache e o
`conan install` roda com `--no-remote`.

## Comandos make

Todo build roda dentro do sandbox. Rode `make help` para a lista completa.

```bash
make setup                            # ativa o hook de pre-commit (uma vez após clonar)
make sandbox-build                    # constrói a imagem (pré-compila as deps do Conan)
make sandbox-shell                    # shell no container, repo em /workspace

make rebuild                          # binário Release -> production-builds/release/
make rebuild-unit && make test-unit   # testes + cobertura (coverage-reports/html/index.html)
make rebuild-asan && make test-asan   # testes com ASan + UBSan
make rebuild-valgrind && make test-valgrind
make rebuild-tsan && make test-tsan   # ver nota abaixo
make test-one-asan PATTERN=<regex>    # um binário de teste; vale também para -unit, -tsan, -valgrind

make format                           # aplica clang-format (format-check só confere)
make tidy-fix                         # aplica clang-tidy (tidy-check só confere); precisa de 'make configure'
```

O TSan aborta com `unexpected memory mapping` em hosts com `vm.mmap_rnd_bits=32`. Rode
`sudo sysctl vm.mmap_rnd_bits=28` no host antes de `make test-tsan`.

O hook de pre-commit executa: imagem do sandbox, `format-check`, release, `tidy-check`, testes unitários, ASan
e Valgrind. Como cada build type funciona e suas armadilhas: [docs/build/](docs/build/).

## Padrão de commit

[Conventional Commits](https://www.conventionalcommits.org/) em inglês: `tipo(escopo): descrição`, com o
escopo opcional.

```
feat(adapter_socket): socket endpoint, listener, deadline and file descriptor
build: port piiptables sandbox build infra
docs: record perf profiling options, deferred until a high-load E2E exists
```

Tipos usados: `feat`, `fix`, `refactor`, `test`, `build`, `docs`, `chore`.

Fluxo de branches: a feature sai da `develop` e volta por squash merge. A `develop` só entra na `master`
depois que o slice passa na validação ponta a ponta do `EndToEnd`. Uma branch só é apagada depois que o
trabalho chega à `master`.

## Padrão de comentário

Todo comentário começa com um **modificador** em maiúsculas, seguido de dois-pontos e uma descrição. Comentário
sem modificador não é aceito, e o modificador sozinho também não.

| Modificador | Significado |
|-------------|-------------|
| `BUG` | problema conhecido que causa comportamento incorreto |
| `COMPLEX` | código complicado que precisa de revisão ou refatoração |
| `FIXME` | problema conhecido que precisa ser corrigido |
| `HACK` | solução não ideal ou contorno temporário |
| `LEARN` | ponto importante e não óbvio, que pede entendimento (use para explicar código) |
| `OPTIMIZE` | trecho que pode ser otimizado |
| `REVIEW` | trecho que merece revisão |
| `TODO` | trabalho ainda por fazer neste bloco |

```cpp
// LEARN: SO_PEERCRED devolve o uid do processo que fez connect(), não de quem abriu o socket.
// TODO: tratar EINTR no poll quando houver tratamento de sinais.
```

São as tags do [Todo Tree](https://github.com/Gruntfuggly/todo-tree), que lista todas no editor. Ficam no
código ao fazer merge, não remova antes.
