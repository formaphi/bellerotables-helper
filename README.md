# iptables-manager

Binario modular em C++23 para gerenciar regras de firewall iptables usando os headers da netfilter e as lib iptc, xtable e etc, substituindo os scripts Perl legados do NGFW (`omne-apply-firewall-input`, `omne-apply-firewall-redir`). Alvo: imagem Yocto `meta-bellerophon` (Scarthgap, kernel 6.6); o desenvolvimento roda no sandbox Ubuntu 24.04 (`sandbox/`).

Cada dominio do firewall e exposto como um comando CLI:

```bash
iptables-manager zoneProtection   # Regras INPUT (controle de acesso por zona)
iptables-manager portForward      # Port forwarding (planejado)
```

## Arquitetura

Clean Architecture com 4 aneis concentricos. Dependencias apontam apenas para dentro.

```
Ring 1 (domain)              — Logica de dominio pura, zero deps externas
Ring 2 (application)         — Port interfaces (input + output), use cases; depende apenas de domain
Ring 3 (interface adapters)  — db-reader, serializer, file-writer
Ring 4 (frameworks/drivers)  — framework/db-driver, framework/command-builder, src/main.cpp
```

## Dependencias

```
C++23, CMake 3.28+, Conan 2.x, GCC 13
gtest/1.14.0      — unit tests + gmock (Conan)
libiptc (ip4tc)   — API C para manipular regras iptables no kernel (apt)
libsystemd        — sd_listen_fds() para a ativacao por socket (apt)
```

## libiptc — Como funciona

Regras iptables sao construidas como um buffer binario contiguo, nao uma arvore de objetos.

### Layout do buffer

```
[ipt_entry header] [match1] [match2] ... [matchN] [target]
|                  |<-------- entry->elems[] -------->|
|<- target_offset ->|                                 |
|<- next_offset -------------------------------------------->|
```

### Fluxo para construir uma regra

1. Calcular tamanhos — cada match + target com `XT_ALIGN(sizeof(header) + sizeof(data))`
2. Calcular `target_offset` (ipt_entry + todos matches) e `next_offset` (+ target)
3. Um unico `calloc` com `next_offset` bytes
4. Preencher `ipt_entry` (proto, src, dst)
5. Para cada match sequencialmente no buffer:
   - Posicionar ptr no offset
   - Preencher header `xt_entry_match` (name, match_size, revision)
   - Preencher struct de dados especifica (ex: `xt_mark_mtinfo1`, `xt_tcp`)
   - Avancar `ptr += match_sz`
6. Preencher target no final (ex: NFLOG, ACCEPT, DROP, MARK)
7. `iptc_append_entry(chain, entry, handle)`
8. `iptc_commit(handle)`

### Tipos de target

Dois layouts binarios distintos — standard vs extension:

| Tipo | Struct | Campo name | Data |
|------|--------|-----------|------|
| Standard (ACCEPT, DROP, RETURN) | `xt_standard_target` | vazio | `int verdict` |
| Extension (NFLOG, DNAT, MARK…) | `xt_entry_target` + data | preenchido | struct especifica |

Verdicts standard: `ACCEPT = -NF_ACCEPT - 1`, `DROP = -NF_DROP - 1`, `RETURN = XT_RETURN`.

`iptc_get_target()` retorna string vazia pra standard targets — ler `xt_standard_target.verdict` pra identificar.

### Design patterns (wrapper Ring 4)

```
Match (Template Method)
  write(): set match_size → set name → set revision → fillData()
  Subclasses: MarkMatch, TcpMatch, UdpMatch, ConntrackMatch, HashlimitMatch
  Protocol match (tcp/udp) declara protocol() → Builder infere ipt_entry.ip.proto

ITarget (Strategy interface)
├── StandardTarget — enum class Verdict { ACCEPT, DROP, RETURN }
└── ExtensionTarget (Template Method)
    write(): set target_size → set name → fillData()
    Subclasses: NflogTarget, (futuro: DnatTarget, MarkTarget…)

RuleBuilder (Builder)
  addMatch() → setTarget() → build()
  Gerencia alocacao do buffer (calloc) e calculo de offsets
```

Arquivos: `iptc_builder.h` (Match + RuleBuilder), `iptc_target.h` (hierarquia ITarget).

### Armadilhas

- Linkar com `ip4tc` (nao `iptc`)
- As distros usam nftables por default — CLI `iptables` nao enxerga tabelas legacy escritas por libiptc sem `iptables-legacy` (o sandbox ja troca)
- `-fpermissive` necessario nos headers do kernel (cast void* em `ipt_get_target`)
- `hashlimit` exige `cfg.gc_interval > 0` (ex: 1000ms), kernel rejeita 0
- `hashlimit` `cfg.avg` para `--hashlimit-upto`: `XT_HASHLIMIT_SCALE * seconds_per_unit / rate` (12/hour → `scale * 300`)
- `xt_tcp` tem campos separados `spts[2]` e `dpts[2]` — nao confundir
- C++ exige cast explicito do retorno de `calloc` (`void*` → `ipt_entry*`), usar `unique_ptr` com `FreeDeleter` pra RAII
- `unsigned char *` obrigatorio pra ponteiros de buffer — dados binarios com nulls, nao texto

### Ambiente Docker para desenvolvimento

Todo build roda dentro do sandbox (`sandbox/`, Ubuntu 24.04). Detalhes em `CLAUDE.md` (Build System) e
`docs/build/`.

```bash
make sandbox-build                    # imagem (pre-compila as deps do Conan)
make rebuild                          # binario Release -> production-builds/release/
make rebuild-unit && make test-unit   # testes + cobertura gcov
make rebuild-asan && make test-asan   # testes com ASan + UBSan
make sandbox-shell                    # shell no container
make help                             # todos os targets
```
