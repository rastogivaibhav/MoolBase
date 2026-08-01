#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
paper_dir="${repo_root}/paper"
out_dir="${paper_dir}/build/arxiv"
stage_dir="${out_dir}/source"
archive="${out_dir}/graphenedb-arxiv-source.tar.gz"
manifest="${out_dir}/graphenedb-arxiv-source.sha256"

for tool in python3 pdflatex tar sha256sum; do
  command -v "${tool}" >/dev/null 2>&1 || {
    echo "missing required tool: ${tool}" >&2
    exit 1
  }
done

if bibtex --version >/dev/null 2>&1; then
  bibtex_bin="bibtex"
elif bibtex.original --version >/dev/null 2>&1; then
  bibtex_bin="bibtex.original"
else
  echo "missing working bibtex executable" >&2
  exit 1
fi

python3 "${repo_root}/scripts/check_arxiv_package.py" --paper-dir "${paper_dir}"

rm -rf "${out_dir}"
mkdir -p "${stage_dir}"

(
  cd "${paper_dir}"
  pdflatex -interaction=nonstopmode -halt-on-error main.tex
  "${bibtex_bin}" main
  pdflatex -interaction=nonstopmode -halt-on-error main.tex
  pdflatex -interaction=nonstopmode -halt-on-error main.tex
)

install -m 0644 "${paper_dir}/main.tex" "${stage_dir}/main.tex"
install -m 0644 "${paper_dir}/references.bib" "${stage_dir}/references.bib"
install -m 0644 "${paper_dir}/main.bbl" "${stage_dir}/main.bbl"

# Reject build residue or unrelated material from the staged submission source.
if find "${stage_dir}" -type f \( -name '*.aux' -o -name '*.log' -o -name '*.blg' -o -name '*.out' -o -name '*.pdf' \) | grep -q .; then
  echo "forbidden build residue found in staged arXiv source" >&2
  exit 1
fi

(
  cd "${stage_dir}"
  sha256sum main.tex references.bib main.bbl > MANIFEST.sha256
  tar -czf "${archive}" main.tex references.bib main.bbl MANIFEST.sha256
)
sha256sum "${archive}" > "${manifest}"

echo "arxiv_source_archive=${archive}"
echo "arxiv_source_checksum=${manifest}"
echo "arxiv_source_package=PASS"
