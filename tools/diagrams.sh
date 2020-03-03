#! /bin/sh

set -eu

SHOWPATH=$(dirname "$0")/show_path.py

for DEV in $(jq '.all_devices | keys[]' documentation/models.json | sed 's/"//g')
do
    OUT="model_${DEV}.ps"
    echo "Model: ${DEV} -> ${OUT}"
    "${SHOWPATH}" --device-id "${DEV}" --dot - documentation/models.json | dot -o "${OUT}" -Tps

    OUT="model_${DEV}.pdf"
    echo "Model: ${DEV} -> ${OUT}"
    "${SHOWPATH}" --device-id "${DEV}" --dot - documentation/models.json | dot -o "${OUT}" -Tpdf
done
