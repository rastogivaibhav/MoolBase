SHELL := /usr/bin/env bash
PAPER := main
BIBTEX := $(shell if bibtex --version >/dev/null 2>&1; then echo bibtex; elif bibtex.original --version >/dev/null 2>&1; then echo bibtex.original; else echo bibtex; fi)

.PHONY: all check pdf arxiv clean

all: check

pdf:
	pdflatex -interaction=nonstopmode -halt-on-error $(PAPER).tex
	$(BIBTEX) $(PAPER)
	pdflatex -interaction=nonstopmode -halt-on-error $(PAPER).tex
	pdflatex -interaction=nonstopmode -halt-on-error $(PAPER).tex

check:
	python3 ../scripts/check_arxiv_package.py --paper-dir .
	$(MAKE) pdf
	chktex -q -n 8 -n 24 $(PAPER).tex || true

arxiv: check
	bash ../scripts/package_arxiv_source.sh

clean:
	rm -f $(PAPER).aux $(PAPER).bbl $(PAPER).blg $(PAPER).log $(PAPER).out $(PAPER).pdf $(PAPER).fls $(PAPER).fdb_latexmk
	rm -rf build
