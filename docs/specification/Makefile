FILE=proposal

all: $(FILE).pdf

$(FILE).pdf: $(FILE).tex
	latex $(FILE).tex
	latex $(FILE).tex
	dvipdfm $(FILE).dvi

clean:
	rm -rf *.aux *.dvi *.log *.pdf *.toc *.lot *.lof
