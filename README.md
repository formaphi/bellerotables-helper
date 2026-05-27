# iptables-manager

Binario modular em C++11 para gerenciar regras de firewall iptables usando os headers da netfilter e as lib iptc, xtable e etc, substituindo os scripts Perl legados do NGFW (`omne-apply-firewall-input`, `omne-apply-firewall-redir`). Roda em AlmaLinux 8 (CentOS 7 ABI).

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
C++11, CMake 3.10+, Conan 1.x
libpq/15.4        — PostgreSQL C client
libiptc
nlohmann_json/3.11.3 — JSON (header-only)
gtest/1.10.0      — unit tests + gmock
Python 3 + pytest — testes de caracterizacao
```
