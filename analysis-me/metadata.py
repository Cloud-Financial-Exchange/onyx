import PyPDF2
import sys

def read_pdf_metadata(pdf_file_path):
    with open(pdf_file_path, 'rb') as file:
        reader = PyPDF2.PdfReader(file)
        metadata = reader.metadata
        return metadata

pdf_file_path = sys.argv[1]  # Replace with your PDF file path
metadata = read_pdf_metadata(pdf_file_path)

# Display the metadata
for key, value in metadata.items():
    print(f'{key}: {value}')
