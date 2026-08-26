# Безопасность EchoBox II

## Официальные обновления

EchoBox II получает обновления только из GitHub Releases репозитория
`BANANCHIKIREAL/EchoBox-II`. Updater проверяет HTTPS-источник, точное имя,
размер и SHA-256 файла до его запуска. При любой ошибке проверка закрывается
без запуска загруженного файла.

Новые релизы, созданные защищённым workflow, публикуют `SHA256SUMS.txt` и
GitHub Artifact Attestations для установщика и portable-архива. Происхождение
файла можно проверить:

```bash
gh attestation verify PATH_TO_FILE -R BANANCHIKIREAL/EchoBox-II
```

Artifact Attestation подтверждает происхождение сборки, но не является
антивирусным заключением. Не отключайте Microsoft Defender или другой
антивирус для установки EchoBox II.

## Сообщение об уязвимости

Не публикуйте детали ещё не исправленной уязвимости в открытом Issue.
Используйте включённый раздел **Security → Report a vulnerability** репозитория.
В сообщении укажите версию EchoBox II, шаги воспроизведения и ожидаемое влияние.
