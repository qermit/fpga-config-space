FILE=proposal

all: $(FILE).pdf 

$(FILE).pdf: $(FILE).tex sdb-h.expand
	latex $(FILE).tex
	latex $(FILE).tex
	dvipdfm $(FILE).dvi

clean:
	rm -rf *.aux *.dvi *.log *.pdf *.toc *.lot *.lof

sdb-h.expand: sdb.h
	expand -8 $^ > $@