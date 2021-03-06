#pragma once

#include <QObject>
#include <QPointer>
#include <QWidget>

namespace PaintField {

class Document;

class DocumentController : public QObject
{
	Q_OBJECT
public:
	explicit DocumentController(Document *document, QObject *parent = 0) : 
		QObject(parent),
		_document(document)
	{}
	
	static QString getOpenSavedFilePath();
	
	/**
	 * Shows a dialog and creates a new document.
	 * @return Null if failed
	 */
	static Document *createFromNewDialog(QWidget *dialogParent);
	
	/**
	 * Creates a document from the clipboard image.
	 * @return Null if failed
	 */
	static Document *createFromClipboard();
	
	static Document *createFromImportDialog();
	
	/**
	 * Opens a file and creates a canvas.
	 * Automatically detects what type the file is (PaintField document or Image file) from the file path, and
	 * calls fromSavedFile or fromImageFile.
	 * @param path
	 * @return Null if failed
	 */
	static Document *createFromFile(const QString &path);
	
	/**
	 * Opens a file as a PaintField .pfield document and opens a canvas.
	 * @param path
	 * @return Null if failed
	 */
	static Document *createFromImageFile(const QString &path);
	
	/**
	 * Opens a file as an image file and creates a new canvas.
	 * @param path
	 * @return Null if failed
	 */
	static Document *createFromSavedFile(const QString &path);
	
	static bool confirmClose(Document *document);
	
	static bool save(Document *document);
	static bool saveAs(Document *document);
	static bool exportToImage(Document *document, QWidget *dialogParent);
	
	void setDialogParent(QWidget *parent) { _dialogParent = parent; }
	
signals:
	
	
	
public slots:
	
	/**
	 * Saves the document
	 * @return 
	 */
	bool save();
	
	/**
	 * Shows a dialog and saves the document into the new file.
	 * @return false if cancelled or failed
	 */
	bool saveAs();
	
	bool exportToImage();
	
private:
	
	Document *_document = 0;
	QPointer<QWidget> _dialogParent;
};

} // namespace PaintField

