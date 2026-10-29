- 23 octber 2023
# Limitações da arquitetura
- O helper é one-shot e não pode ficar pendurado se o agente sumir. Por isso a espera tem um prazo em
    milissegundos.
- O helper não cria o socket. O systemd o cria (.socket com Accept=no) e passa o fd 3 ao processo.

# Versão: 1

# Erros de design
1. FramedConnection deveria ter uma interface e UnixSocketConnection não. O motivo é que o Frammed é a classe mais exposta, é ele que deve ser a adaptação.
2. readPeerCredentials pode ser função interna de Peer

# Dúvidas
1. Pra que Listener é necessário?
2. Deadline e activation poderia não poderiam ser apenas classes usadas por unix_socket_connection?
3. Quais componentes são importantes para o funcionamento desse client socket? conexão, activação, escuta e o que mais?
4. Deadline usa epoll?

# Melhorias nas skills
1. Interfaces não devem possuir construtores.
2. Condições muito grandes devem ser evitadas, sendo trocadas por variáveis
evitar:
if (::setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &socketTimeout, sizeof(socketTimeout)) ==
        SYSCALL_ERROR)

# Versão: 2

# Mudanças de design
1. AgentUserPolicy::isAllowed é método anêmico, ou removemos ou colocamos a lógica dentro de AgentUserPolicy

2.FramedConnection::peerCredentials() também é anêmico.

3. UnixSocketConnection::readPeerCredentials() poderia ser do próprio Peer, mas não é, pois o Peer não tem acesso ao fd do socket. Talvez seja melhor passar o fd para o Peer ou criar um DTO que continha o fd e as credenciais do peer, o deadline (como struct) e outras informações (vale experimentar). Isso tem na skill de DTO

4. Parse do peer poderia ser feito internamente no Peer. Para mitigar isso:
    PeerCredentials peer;
    peer.userId = credentials.uid;
    peer.groupId = credentials.gid;
    peer.processId = static_cast<uint32_t>(credentials.pid);
Talvez pudessemos fazer um método to_peer_credentials() que retornasse um PeerCredentials, similar ao to_json(). Isso tem na skill de json

5. UnixSocketConnection::closeFd() poderia ser o próprio destrutor

6. FramedConnection::writeFrame poderia montar um DTO e passar em writeAll, ao invés de passar os parâmetros no vector. Dentro do DTO poderia ser feito o parse do frame.

7. AgentUserPolicy::resolveUserId e AgentUserPolicy::readConfiguredUserName poderia trabalhar sob o mesmo DTO; talvez até não precisasse dessa classe por isso.

# Dúvidas

# Melhorias nas skills
- colocar condição para restrigir a criação de métodos anêmicos, como esses que retornam apenas um valor de uma variável privada. Sugestão será refatorar o código para que a lógica fique dentro da classe, ou criar um DTO para encapsular os dados e passar para o método como parâmetro, ao invés de um método anêmico em todas as classes para consultar um valor de uma variável privada.
- Se for preciso para testes unitários, Você pode criar um método anêmico com as guards
```cpp
#ifndef UNIT_TEST
    // método anêmico
#endif
```

---

## Respostas à Versão 2

### Mudanças de design

1. **`AgentUserPolicy::isAllowed` anêmico** — aplicado, com ajuste. `isAllowed` não é anêmico: tem lógica e é o
   único método de `IPeerPolicy`, que o endpoint usa. O anêmico era `allowedUserId()`, usado só pelos testes;
   foi removido e os testes passaram a verificar pelo comportamento (`isAllowed` com o uid configurado e com
   um uid estranho).
2. **`FramedConnection::peerCredentials()` anêmico** — aplicado. Saiu de `IFramedConnection`, `FramedConnection`
   e `MockFramedConnection`: o endpoint valida o peer antes de criar a conexão com frames, e ninguém depois dele
   precisa das credenciais (era a pendência de ISP em `socket-endpoint.md`). `ISocketConnection::peerCredentials()`
   fica, porque o endpoint a usa.
3. **fd no Peer ou DTO com fd + credenciais + deadline** — não aplicado; a intenção ficou coberta pelo item 4.
   - O fd é um recurso RAII de `UnixSocketConnection`; um `struct` copiável com o fd abre caminho para
     double-close ou fd pendurado.
   - A deadline é recalculada a cada `readExactly`/`writeAll`, não pertence à conexão.
   - A `cpp-dto` proíbe lógica de negócio no DTO; um `getsockopt` com errno → exceção é I/O.
   A syscall fica com quem é dono do fd; a conversão foi para o DTO.
4. **`to_peer_credentials()` como `to_json()`** — aplicado como `PeerCredentials::fromUcred(const ucred&)`.
   `to_json` é função livre (ADL), o que viola a regra de não ter funções livres; o helper estático é a forma
   que a `cpp-dto` prevê (`parse`/`fromBytes`). `readPeerCredentials()` ficou só com o `getsockopt`, e a
   conversão é testável sem socket.
5. **`closeFd()` como o próprio destrutor** — adiado para a reformulação do `FileDescriptor`. `closeFd()` também
   é chamado no `catch` do construtor (e na validação de timeout do listener), e quando o construtor lança o
   destrutor não roda. Com um `FileDescriptor` RAII como membro, o destrutor do membro roda mesmo se o
   construtor externo lançar, e `closeFd()` desaparece nas duas classes.
6. **`writeFrame` montar um DTO** — aplicado como `FrameHeaderDto` (`parse` + `toBytes`, igual ao `DnsHeaderDto` da
   skill): a codificação big-endian, as constantes de shift/índice e a checagem de tamanho máximo saíram de
   `FramedConnection`. `writeAll` continua recebendo bytes: se `ISocketConnection` conhecesse frames, a camada de
   baixo dependeria da de cima e o Adapter perderia o sentido.
7. **`resolveUserId` + `readConfiguredUserName` sob o mesmo DTO** — aplicado em parte, como `HelperConfigDto::parse`.
   A leitura do `key=value` (`trim`, comentários, chave `agent_user`) virou um DTO puro, testável sem arquivo,
   onde entram as próximas chaves do `.conf`. A classe `AgentUserPolicy` não pode sumir: é a Strategy que
   implementa `IPeerPolicy`, injetada no endpoint. Ela lê o arquivo, chama `parse` e resolve o uid.

### Melhorias nas skills

- **Métodos anêmicos** — nova seção "No anemic methods" na `cpp-concrete-class` e um item na lista de regras
  mais esquecidas da `cpp-style`. A regra mira getters que só devolvem um membro e repasses puros; uma consulta
  de interface que só o objeto sabe responder e que o cliente precisa (`ISocketConnection::peerCredentials()`)
  não é anêmica.
- **Guard `UNIT_TEST`** — o snippet da Versão 2 estava invertido: com `#ifndef UNIT_TEST` o método some dos
  testes e vai para produção. A seção da `cpp-concrete-class` agora diz:
  - testar primeiro pelo comportamento público;
  - `#ifdef UNIT_TEST`, método **inline no header** (`UNIT_TEST` só é definido no executável de testes; um
    método guardado definido em `src/` dá *undefined reference*);
  - o guard só adiciona métodos, nunca membros de dados nem virtuais (layouts diferentes entre lib e
    testes = violação de ODR).

---

# Versão: 3
### Mudanças de design
**fd no Peer ou DTO com fd + credenciais + deadline** : se o problema é a struct copiável com fd, podemos criar um DTO que contenha apenas as credenciais do peer e a deadline, sem o fd. Ou podemos deletar o constructor de cópia do DTO, para que ele não seja copiável; permitindo apenas a movimentação (move) do DTO.
---

## Respostas à Versão 3

### Mudanças de design

**fd no Peer ou DTO com fd + credenciais + deadline (item 3 da Versão 2)**

- **(a) DTO só com credenciais + deadline, sem o fd** — não recomendado. Tira o problema do fd, mas junta dados
  com tempos de vida e direções diferentes:
  - as credenciais são lidas **uma vez**, quando a conexão nasce, e **saem** dela para o endpoint;
  - a deadline é recalculada **a cada operação** (`now() + m_timeout` em cada `readExactly`/`writeAll`); uma
    deadline guardada por conexão vence já na segunda operação.
  O que é por conexão é o **timeout**, e ele é configuração que **entra**, não resultado que sai.
- **(b) DTO move-only com o fd** — a `cpp-dto` prevê DTOs move-only, mas para um `int` não basta: o move padrão
  de um `int` **copia** o valor e não zera a origem, e um struct não fecha o fd ao ser destruído. Os dois objetos
  ficariam com o mesmo fd. Para ser seguro, o membro precisa de um move que zere a origem (`-1`) e de um
  destrutor que feche o fd — exatamente a classe RAII `FileDescriptor` do item 5.
- **Convergência** — a ideia funciona **depois** do `FileDescriptor`: um DTO que guarda um `FileDescriptor` por
  valor vira move-only automaticamente e o fd fecha sozinho. Os itens 3 e 5 são o mesmo redesign e serão
  tratados juntos.

---

# Versão 4

### Mudanças de código
- UnixSocketListener::acceptOne() poderia receber os timeouts, ao invés de serem passados no construtor
- FileDescriptor::operationName(Readiness readiness) é função boiler plate
- passwd* found pode ser unique_ptr


### Mudanças de design
- UnixSocketConnection::configureTimeouts não deveria ser trabalho de UnixSocketConnection.
- readPeerCredentials() poderia ser do próprio Peer, mas não é, pois o Peer não tem acesso ao fd do socket. Ou fazemos uma classe Peer que tenha o IFileDescriptor, ou criamos um DTO.
- timeout poderia ser DTO?

## Skills
- O agent dá preferência a funções sem parâmetros, que usam o estado do objeto. Isso leva a métodos anêmicos, que são desencorajados. A solução é criar DTOs que encapsulem os dados e passem para o método como parâmetro, ao invés de um método anêmico em todas as classes para consultar um valor de uma variável privada.
Exemplo: UnixSocketConnection::readPeerCredentials() poderia receber um DTO com o fd via move e retornar outro DTO com as credenciais do peer, ao invés de ser um método anêmico que retorna apenas as credenciais do peer.

## Dúvidas
- Os loops while que temos poderiam ser substituídos por future/promise? Ou por corrotinas? Ou por async/await? Ou por select/poll/epoll?
---

## Respostas à Versão 4

### Mudanças de design

1. **`configureTimeouts` não é trabalho de `UnixSocketConnection`** — aplicado, e foi além: o método é **removido**, não
   movido. `SO_RCVTIMEO`/`SO_SNDTIMEO` só valem para chamadas bloqueantes, mas todo `recv`/`send` usa `MSG_DONTWAIT`
   depois do `poll`. Os timeouts de kernel nunca disparavam; o prazo já é imposto por `IDeadline` + `poll`
   ([socket-deadline.md](socket-deadline.md)). Saem duas syscalls, a conversão para `timeval` e um segundo
   mecanismo de tempo.
2. **`readPeerCredentials()` no Peer ou num DTO com o fd** — nenhum dos dois; as credenciais vão para o **accept**.
   - No `PeerCredentials` (DTO): `getsockopt` é I/O com errno → exceção, proibido pela `cpp-dto`.
   - Numa classe Peer dona do `IFileDescriptor`: o fd pertence à conexão, que ainda lê e escreve nele; um move o
     tiraria dela.
   - As credenciais descrevem *quem conectou*, um fato que nasce no `accept4`. O listener lê `SO_PEERCRED` logo
     após aceitar (método estático privado com `const IFileDescriptor&`, empréstimo, sem move) e devolve
     `AcceptedConnectionDto { connection, peer }`. `UnixSocketConnection` perde `readPeerCredentials`,
     `m_peerCredentials` e o getter `peerCredentials()` — o último método anêmico da Versão 2 — e fica só com I/O de
     bytes. O endpoint lê `accepted.peer`.
3. **Timeout como DTO** — aplicado como `SocketTimeoutsDto { acceptTimeout, connectionTimeout }` com
   `create(...)` estático que valida (`<= 0` → `std::invalid_argument`). A regra estava repetida em
   `UnixSocketListenerFactory`, `UnixSocketListener` e `UnixSocketConnection`; agora vive num lugar só. Quem o
   recebe é o `SocketEndpoint`. É DTO de
   configuração; a deadline continua classe (ver [socket-deadline.md](socket-deadline.md), "Why a class").

### Mudanças de código

1. **`acceptOne()` receber os timeouts** — aplicado um nível abaixo: `acceptOne(const IDeadline&)` recebe a
   **deadline** já aberta, não o timeout. O mesmo vale para `readExactly`/`writeAll`, que passam a receber
   `const IDeadline&`. Motivo — um bug: `readFrame` chamava `readExactly` duas vezes (cabeçalho e payload) e cada
   chamada abria sua própria deadline, então um frame podia levar **2 × timeout**. Agora `FramedConnection` abre
   **uma** deadline por frame. Consequências:
   - quem decide o que é "uma operação" abre a deadline: `SocketEndpoint` para o accept (`acceptTimeout`),
     `FramedConnection` para cada frame (`connectionTimeout`);
   - `SocketEndpoint` guarda o `IDeadlineFactory` e o `SocketTimeoutsDto`;
   - `UnixSocketListener`, `UnixSocketListenerFactory` e `UnixSocketConnection` perdem timeout, deadline factory e
     validação — ficam só com o fd.
2. **`FileDescriptor::operationName` é boilerplate** — aplicado. O mapa JSON existia só para compor uma mensagem de
   erro e puxava `<nlohmann/json.hpp>` para um header público. `waitUntilReady` passa a lançar
   `std::system_error(ETIMEDOUT, "poll deadline exceeded")` — o errno continua o que os chamadores já tratam; quem
   chama sabe qual operação era. Saem `operationName`,
   o `NLOHMANN_JSON_SERIALIZE_ENUM` e o `readiness_test.cpp`. `Readiness::Acceptable` fica: ainda documenta a
   intenção no listener, mesmo mapeando para `POLLIN`.
3. **`passwd* found` como `unique_ptr`** — não aplicado; seria um bug. `getpwnam_r` grava em `found` o endereço de
   `entry`, que está na stack. O ponteiro não possui nada: um `unique_ptr` chamaria `delete` em memória de stack
   (UB). Ponteiro cru sem posse é o tipo correto. Única simplificação: ler `found->pw_uid`.

### Skills

- **"Preferir parâmetros a estado"** — aceito, com um critério para a skill:
  - **construtor**: colaboradores e configuração que valem a vida toda do objeto (deadline factory, policy);
  - **parâmetro**: dados da chamada (a deadline da operação, os timeouts do accept);
  - **sinal de anêmico**: estado guardado só para ser lido de volta (`m_peerCredentials` + `peerCredentials()`).
    A correção é devolver o dado onde ele nasce (design 2), não criar o getter.
  - Ajuste ao exemplo: passar o fd **por move** para ler credenciais o consome, e a conexão ainda precisa dele. O
    que só é usado durante a chamada vai por `const IFileDescriptor&`; move só quando a posse muda de verdade
    (activation → listener → connection).

### Dúvidas

- **Trocar os `while` por future/promise, corrotinas, async ou select/poll/epoll?** Os loops **já são** a opção poll.
  As outras não se pagam num processo one-shot, de uma conexão e uma thread:
  - `std::async`/`future` criam threads e não cancelam um `recv` bloqueado; um `wait_for` que expira deixa a thread
    presa;
  - corrotinas C++20 precisam de executor/event loop, que a biblioteca padrão não tem (`std::execution` é C++26);
  - `epoll` compensa com muitos fds; com um só custa um fd a mais e não ganha nada sobre `poll`.
  Os loops existem por leituras/escritas parciais e `EINTR`, que fazem parte de qualquer stream socket.

### Fora do review

- O fd de escuta vem do systemd **bloqueante**. Se o cliente abortar entre o `poll` e o `accept4`, o `accept4` pode
  bloquear sem prazo (`SOCK_CLOEXEC` não muda o listener). Correção aplicada: `SystemdActivation` liga `O_NONBLOCK`
  no fd ativado (`fcntl`), independente do `.socket` ter `NonBlocking=yes`.
