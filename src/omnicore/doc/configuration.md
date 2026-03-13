Configuration
=============

OmniLite can be configured by providing one or more optional command-line arguments:
```bash
$ omnilited -setting=value -setting=value
```

All settings can alternatively also be configured via the `litecoin.conf`.

Depending on the operating system, the default locations for the configuration file are:

- Unix systems: `$HOME/.litecoin/litecoin.conf`
- Mac OS X: `$HOME/Library/Application Support/Litecoin/litecoin.conf`
- Microsoft Windows: `%APPDATA%/Litecoin/litecoin.conf`

A typical `litecoin.conf` may include:
```
server=1
rpcuser=litecoinrpc
rpcpassword=password
rpcallowip=127.0.0.1
rpcport=9332
datacarriersize=80
logtimestamps=1
omnidebug=tally
omnidebug=packets
omnidebug=pending
```

## Optional settings

To run and use OmniLite, no explicit configuration is necessary.

More information about the general configuration and Litecoin Core specific options are available in the [Bitcoin wiki](https://en.bitcoin.it/wiki/Running_Bitcoin).

#### General options:

| Name                         | Type         | Default        | Description                                                                     |
|------------------------------|--------------|----------------|---------------------------------------------------------------------------------|
| `startclean`                 | boolean      | `0`            | clear all persistence files on startup; triggers reparsing of Omni transactions |
| `omnitxcache`                | number       | `500000`       | the maximum number of transactions in the input transaction cache               |
| `omniprogressfrequency`      | number       | `30`           | time in seconds after which the initial scanning progress is reported           |
| `omnishowblockconsensushash` | number       | `0`            | calculate and log the consensus hash for the specified block                    |
| `experimental-ltc-balances`  | boolean      | `0`            | maintain a full address index to query any Litecoin balance                      |

#### Log options:

| Name                         | Type         | Default        | Description                                                                     |
|------------------------------|--------------|----------------|---------------------------------------------------------------------------------|
| `omnilogfile`                | string       | `omnicore.log` | the path of the log file (in the data directory per default)                    |
| `omnidebug`                  | multi string | `""`           | enable or disable log categories, can be `"all"`, `"none"`                      |

#### Transaction options:

| Name                         | Type         | Default        | Description                                                                     |
|------------------------------|--------------|----------------|---------------------------------------------------------------------------------|
| `autocommit`                 | boolean      | `1`            | enable or disable broadcasting of transactions, when creating transactions      |
| `datacarrier`                | boolean      | `1`            | if disabled, payloads are embedded multisig, and not in `OP_RETURN` scripts     |
| `datacarriersize`            | number       | `80`           | the maximum size in byte of payloads embedded in `OP_RETURN` scripts            |

**Note:** the options `-datacarrier` and `datacarriersize` affect the global relay policies of transactions with `OP_RETURN` scripts.

#### RPC server options:

| Name                         | Type         | Default        | Description                                                                     |
|------------------------------|--------------|----------------|---------------------------------------------------------------------------------|
| `rpcforceutf8`               | boolean      | `1`            | replace invalid UTF-8 encoded characters with question marks in RPC responses   |

#### User interface options:

| Name                         | Type         | Default        | Description                                                                     |
|------------------------------|--------------|----------------|---------------------------------------------------------------------------------|
| `disclaimer`                 | boolean      | `0`            | explicitly show QT disclaimer on startup                                        |
| `omniuiwalletscope`          | number       | `65535`        | max. transactions to show in trade and transaction history                      |

#### Alert and activation options:

| Name                         | Type         | Default        | Description                                                                     |
|------------------------------|--------------|----------------|---------------------------------------------------------------------------------|
| `overrideforcedshutdown`     | boolean      | `0`            | overwrite shutdown, triggered by an alert                                       |
| `omnialertallowsender`       | multi string | `""`           | whitelist senders of alerts, can be `"any"`                                     |
| `omnialertignoresender`      | multi string | `""`           | ignore senders of alerts                                                        |
| `omniactivationallowsender`  | multi string | `""`           | whitelist senders of activations                                                |
| `omniactivationignoresender` | multi string | `""`           | ignore senders of activations                                                   |

**Note:** alert and activation related options are consensus affecting and should only be used for tests or under exceptional circumstances!
