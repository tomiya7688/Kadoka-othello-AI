# State / Dataset Format

## Cell値

- `0`: Empty
- `1`: Black
- `2`: White

## GameSnapshot JSON

初期Dataset出力は1行1局面のJSON Lines形式。
各着手後のSnapshotを1行として出力する。

例:

```json
{"board_size":8,"current_player":"white","status":"playing","cells":[0,0,0],"legal_moves":[{"row":2,"col":2}],"history":[{"player":"black","row":2,"col":3}],"result":null}
```

主なフィールド:

- `board_size`: 盤面の一辺
- `current_player`: 次の手番
- `status`: `playing` / `finished`
- `cells`: row-majorの盤面配列
- `legal_moves`: 現手番の合法手
- `history`: ここまでの合法着手履歴
- `result`: 終局前はnull、終局後は石数とwinner

## 注意

JSON Linesは初期実装とデバッグ・相互運用向け。
大量学習データ生成ではI/O容量がボトルネックになるため、将来的に固定長バイナリや圧縮形式を追加する。

Core内部の盤面表現とDataset形式は分離し、出力形式を追加してもゲームルール実装へ影響させない。
