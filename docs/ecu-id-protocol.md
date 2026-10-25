# Protocolo de teste: identificação da ECU (ZS125GY-13-E55)

Objetivo: saber quem fez a centralina (ECU). Está etiquetada FAI / New Means
85400-1614, mas a caixa é igual à de uma Bosch MSE6.0. Se for uma Bosch por
baixo, há ferramentas comerciais que a leem e reprogramam. As ECUs Bosch
denunciam-se nos dados de identificação: fornecedor "BOSCH", números de
hardware do tipo `0 261 S0x xxx` ou `F 01R ...`, números de software a
começar por `1037`.

## O que o firmware faz

O comando `ECU ID` (por BLE ou escrito no monitor série) pede, por ordem:

1. **OBD modo 09** no ID funcional `0x7DF`: PIDs suportados, VIN, calibration
   ID (CALID), CVN e nome da ECU.
2. **UDS** no ID físico da ECU, `0x7E0`: `3E 00` (a ECU fala neste ID?),
   sessão ativa (`22 F186`) e os identificadores `22 F180`..`F197` (número de
   peça, software, fornecedor, hardware, data, série, VIN, nome do sistema).
3. **KWP2000** `1A 80`..`1A 9B` (identificação usada pelas Bosch mais antigas).
4. Três identificadores UDS também no ID funcional, caso o físico não seja
   `0x7E0`.
5. **Só com o motor parado:** sessão estendida (`10 03`) e pedido de seed nos
   níveis de segurança `27 01`, `03`, `05` e `11`, para saber que níveis
   existem e o tamanho da seed. No fim volta à sessão normal (`10 01`). Com
   o motor a trabalhar estes passos aparecem como `SKIPPED`.

**É só leitura.** Nunca envia uma chave (chaves erradas podem bloquear a ECU),
nunca entra em sessão de programação (`10 02`), não escreve, não corre rotinas
e não faz reset. O painel continua a funcionar durante o teste. Demora entre
10 s e 1 min (cada pedido sem resposta espera 1,5 s).

## Preparação

- Gravar esta versão do firmware.
- ESP32 ligado ao CAN da mota (como sempre) **e** ao PC pelo USB (CH343,
  `COM8`), ou a um telemóvel por BLE.
- Ignição ligada, **motor parado**, mota parada.

## Correr o teste

Pelo PC:

```
idf.py -p COM8 monitor | tee ecu_id.log
```

Esperar pelo `Ready.` e escrever `ECU ID` + Enter. As respostas aparecem no
monitor, todas a começar por `ECUID`, até `ECUID END`.

Pelo telemóvel: app de terminal BLE ligada a `OBDII`, enviar `ECU ID`.

Enviar no fio do projeto o ficheiro `ecu_id.log` (ou o texto copiado).

## Segundo passo: ECU PROBE (depois de ECU ID)

Depois de o `ECU ID` mostrar que a ECU fala UDS em 0x7E0, o comando `ECU PROBE`
vai mais fundo, sempre só com leitura e sem motor:

1. Abre a sessão estendida (`10 03`).
2. Lê a seed de segurança (`27 01`) **três vezes** seguidas. Se der sempre
   `56 78`, a seed é fixa dentro da sessão. Corre o comando **outra vez depois
   de desligar e ligar a chave** para ver se também não muda entre arranques:
   uma seed fixa quer dizer chave fixa e proteção fraca.
3. Tenta ler memória com `ReadMemoryByAddress` (`23`) em três formatos de
   endereço, e pedir um `RequestUpload` (`35`). Ambos são leitura (o upload é a
   ECU a mandar memória para fora, não a receber). As respostas dizem tudo:
   - `NEGATIVE 33 securityAccessDenied` = o serviço existe mas precisa da chave;
   - `NEGATIVE 31 requestOutOfRange` = o serviço está aberto, só o endereço é
     que está errado (ótimo sinal, dá para procurar o mapa de memória);
   - `OK` = lê memória sem chave nenhuma (o melhor cenário para um dump);
   - `NEGATIVE 11 serviceNotSupported` = não tem esse serviço.
4. Volta à sessão normal (`10 01`).

Nunca envia uma chave, nem entra em programação, nem escreve. Corre assim:

```
idf.py -p COM9 monitor | tee ecu_probe.log
```

Escrever `ECU PROBE` + Enter, esperar por `ECUID END`, e correr de novo depois
de uma volta à chave.

## Como ler o resultado

```
ECUID START steps=42 physical=7E0 engine=stopped
ECUID F 0904 calibration ID: OK 20 bytes
ECUID   49 04 01 31 30 33 37 35 34 31 32 33 34 00 00 00  |I..1037541234...|
ECUID P 22F18A system supplier: OK 9 bytes
ECUID   62 F1 8A 42 4F 53 43 48 00                       |b..BOSCH.|
ECUID P 22F191 ECU hardware number: NEGATIVE 31 requestOutOfRange
ECUID P 2701 seed level 01: OK 6 bytes
ECUID   67 01 3A 9F 12 C4                                |g.:...|
ECUID P 1A9B KWP Bosch calibration ID: NO REPLY
ECUID END answered=30 of 42
```

- `F` / `P`: pedido no ID funcional (`0x7DF`) ou físico (`0x7E0`).
- `OK n bytes` e depois a resposta em hexadecimal, com o texto legível entre
  `| |` (os outros bytes aparecem como `.`).
- `NEGATIVE xx nome`: a ECU recusou. `11` = serviço não existe, `12`/`31` =
  identificador não existe, `22` = condições erradas, `33` = acesso negado,
  `7F` = serviço não disponível nesta sessão.
- `NO REPLY`: nada respondeu. Se **todos** os `P` derem `NO REPLY`, a ECU não
  usa `0x7E0`; ver os passos `F` do ponto 4.

O que procurar:
- Texto `BOSCH`, um número `0 261 ...` / `F 01R ...`, ou um software `1037...`
  em qualquer resposta: é uma Bosch.
- Respostas positivas a `27 xx`: mostram os níveis de segurança e o tamanho
  da seed (útil para uma futura reprogramação).
