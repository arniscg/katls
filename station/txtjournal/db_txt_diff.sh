DB=/mnt/pechka-backups/$(ls /mnt/pechka-backups -t | head -n 1)
TXT=$(pwd)/journal.txt
TXT_FROM_DB=$(pwd)/journal_from_db.txt

echo comparing $DB and $TXT

python3 ./db_to_txt_journal.py $DB $TXT_FROM_DB

git difftool --no-index $TXT $TXT_FROM_DB