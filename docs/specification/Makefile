INPUT = $(wildcard *.tex)
ALL = $(INPUT:.tex=.pdf)

all: $(ALL) 

%.pdf: %.tex sdb-h.expand
	latex $*.tex
	latex $*.tex
	dvipdfm $*.dvi

clean:
	rm -rf *.aux *.dvi *.log *.pdf *.toc *.lot *.lof

sdb-h.expand: sdb.h
	expand -8 $^ > $@
