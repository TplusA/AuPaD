PANDOC_PDF = pandoc --latex-engine=lualatex -V urlcolor=blue -V papersize:a4

all: protocol.pdf

clean:
	rm -f protocol.pdf

protocol.pdf: protocol.md
	$(PANDOC_PDF) --toc -N -o $@ $<
