FILE=sdwb

all:
	latex $(FILE).tex
	latex $(FILE).tex
	dvipdfm $(FILE).dvi
	evince $(FILE).pdf

clean:
	rm -rf *.aux *.dvi *.log *.pdf
